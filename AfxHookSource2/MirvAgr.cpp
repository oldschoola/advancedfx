#include "stdafx.h"
#include "MirvAgr.h"
#include "Globals.h"
#include <fstream>
#include <string>
#include "ClientEntitySystem.h"
#include "MirvTime.h"
#include "WrpConsole.h"
#include <cstring>
#include <map>
#include <vector>
#include <sstream>
#include <cmath>
// #include "../shared/MirvInput.h" // This does not define MirvInputEx
// #include "main.cpp" // This causes multiple definition errors

// Instead, as a workaround, forward declare the class and use extern for the global:
class MirvInputEx {
public:
    MirvInputEx();
    ~MirvInputEx();
    class MirvInput * m_MirvInput;
    double LastCameraOrigin[3];
    double LastCameraAngles[3];
    double LastCameraFov;
    double GameCameraOrigin[3];
    double GameCameraAngles[3];
    double GameCameraFov;
    double LastFrameTime;
    int LastWidth;
    int LastHeight;
};
extern MirvInputEx g_MirvInputEx;

MirvAgr g_MirvAgr;

MirvAgr::MirvAgr() {}
MirvAgr::~MirvAgr() { Stop(); }

void MirvAgr::Start(const char* filename) {
    if (m_Recording) return;
    m_File = new std::ofstream(filename, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!m_File || !static_cast<std::ofstream*>(m_File)->is_open()) {
        advancedfx::Warning("mirv_agr: Failed to open file for writing.\n");
        delete static_cast<std::ofstream*>(m_File);
        m_File = nullptr;
        return;
    }
    m_Recording = true;
    WriteHeader();
    advancedfx::Message("mirv_agr: Recording started to %s.\n", filename);
}

void MirvAgr::Stop() {
    if (!m_Recording) return;
    WriteFooter();
    if (m_File) {
        static_cast<std::ofstream*>(m_File)->close();
        delete static_cast<std::ofstream*>(m_File);
        m_File = nullptr;
    }
    m_Recording = false;
    advancedfx::Message("mirv_agr: Recording stopped.\n");
}

bool MirvAgr::IsRecording() const { return m_Recording; }

// Helper for dictionary-based string writing
class AgrDictWriter {
public:
    AgrDictWriter(std::ofstream* file) : m_File(file) {}
    int WriteString(const std::string& str) {
        auto it = m_Dict.find(str);
        if (it == m_Dict.end()) {
            int32_t minusOne = -1;
            m_File->write(reinterpret_cast<const char*>(&minusOne), sizeof(minusOne));
            m_File->write(str.c_str(), str.size() + 1); // null-terminated
            int idx = (int)m_Dict.size();
            m_Dict[str] = idx;
            return idx;
        } else {
            int32_t idx = it->second;
            m_File->write(reinterpret_cast<const char*>(&idx), sizeof(idx));
            return idx;
        }
    }
    void Reset() { m_Dict.clear(); }
private:
    std::map<std::string, int> m_Dict;
    std::ofstream* m_File;
};

void MirvAgr::CaptureFrame() {
    if (!m_Recording || !m_File) return;
    std::ofstream* file = static_cast<std::ofstream*>(m_File);
    static AgrDictWriter dictWriter(file);
    static bool firstFrame = true;
    if (firstFrame) {
        dictWriter.Reset();
        firstFrame = false;
    }
    
    // Write afxFrame token
    dictWriter.WriteString("afxFrame");
    float frameTime = g_MirvTime.frametime_get();
    file->write(reinterpret_cast<const char*>(&frameTime), sizeof(frameTime));
    
    // Write afxHiddenOffset (must be present for the importer to work properly)
    int32_t afxHiddenOffset = (int32_t)file->tellp() + 4; // Position after this int32
    file->write(reinterpret_cast<const char*>(&afxHiddenOffset), sizeof(afxHiddenOffset));
    
    // Write hidden entities count (0 since we're not hiding anything yet)
    int32_t numHidden = 0;
    file->write(reinterpret_cast<const char*>(&numHidden), sizeof(numHidden));

    // --- CAMERA BLOCK (main camera, not entity) ---
    if (m_RecordCamera) {
        dictWriter.WriteString("afxCam");
        
        // Camera position
        float pos[3];
        pos[0] = (float)g_MirvInputEx.LastCameraOrigin[0];
        pos[1] = (float)g_MirvInputEx.LastCameraOrigin[1];
        pos[2] = (float)g_MirvInputEx.LastCameraOrigin[2];
        file->write(reinterpret_cast<const char*>(&pos[0]), sizeof(float));
        file->write(reinterpret_cast<const char*>(&pos[1]), sizeof(float));
        file->write(reinterpret_cast<const char*>(&pos[2]), sizeof(float));
        
        // Camera angles
        float ang[3];
        ang[0] = (float)g_MirvInputEx.LastCameraAngles[0];
        ang[1] = (float)g_MirvInputEx.LastCameraAngles[1];
        ang[2] = (float)g_MirvInputEx.LastCameraAngles[2];
        file->write(reinterpret_cast<const char*>(&ang[0]), sizeof(float));
        file->write(reinterpret_cast<const char*>(&ang[1]), sizeof(float));
        file->write(reinterpret_cast<const char*>(&ang[2]), sizeof(float));
        
        // Camera FOV
        float fov = (float)g_MirvInputEx.LastCameraFov;
        // Ensure FOV is never zero or too small to avoid division by zero in the Python importer
        if (fov <= 0.01f || fov >= 179.9f) {
            fov = 90.0f; // Use a safe default FOV value
        }
        file->write(reinterpret_cast<const char*>(&fov), sizeof(fov));
    }

    // --- ENTITY BLOCKS ---
    EntityListIterator it;
    int highestIndex = g_GetHighestEntityIterator(*g_pEntityList, &it)->GetIndex();
    for (int i = 0; i <= highestIndex; ++i) {
        if (auto ent = (CEntityInstance*)g_GetEntityFromIndex(*g_pEntityList, i)) {
            const char* className = ent->GetClassName();
            
            // Skip if not recording the appropriate entity type
            bool isPlayer = ent->IsPlayer();
            bool isWeapon = className && strstr(className, "weapon");
            bool isProjectile = className && (
                strstr(className, "grenade") || 
                strstr(className, "projectile") || 
                strstr(className, "rocket") || 
                strstr(className, "bomb")
            );
            bool isViewModel = className && strstr(className, "viewmodel");
            
            if ((isPlayer && !m_RecordPlayers) ||
                (isWeapon && !m_RecordWeapons) ||
                (isProjectile && !m_RecordProjectiles) ||
                (isViewModel && !m_RecordViewmodel && !m_RecordViewmodels)) {
                continue;
            }
            
            // Write entity_state
            dictWriter.WriteString("entity_state");
            file->write(reinterpret_cast<const char*>(&i), sizeof(int32_t));
            
            // Write baseentity
            dictWriter.WriteString("baseentity");
            
            // Get class name for model name
            dictWriter.WriteString(className ? className : "unknown");
            
            // Entity visible
            bool visible = true;
            file->write(reinterpret_cast<const char*>(&visible), sizeof(visible));
            
            // Get position and angles
            float pos[3] = {0,0,0};
            ent->GetOrigin(pos[0], pos[1], pos[2]);
            float ang[3] = {0,0,0};
            ent->GetRenderEyeAngles(ang);
            
            // Calculate rotation matrix
            float matrix[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
            // Fill translation part
            matrix[3] = pos[0];
            matrix[7] = pos[1];
            matrix[11] = pos[2];
            
            // Write the matrix
            file->write(reinterpret_cast<const char*>(matrix), sizeof(matrix));
            
            // --- baseanimating ---
            dictWriter.WriteString("baseanimating");
            
            // Write bone data if this is a player
            bool hasBoneList = false;
            if (isPlayer) {
                int boneCount = ent->GetBoneCount();
                if (boneCount > 0) {
                    hasBoneList = true;
                    file->write(reinterpret_cast<const char*>(&hasBoneList), sizeof(hasBoneList));
                    int32_t numBones = boneCount;
                    file->write(reinterpret_cast<const char*>(&numBones), sizeof(numBones));
                    
                    for (int b = 0; b < boneCount; b++) {
                        float boneMatrix[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
                        ent->GetBoneMatrix(b, boneMatrix);
                        file->write(reinterpret_cast<const char*>(boneMatrix), sizeof(boneMatrix));
                    }
                } else {
                    file->write(reinterpret_cast<const char*>(&hasBoneList), sizeof(hasBoneList));
                }
            } else {
                file->write(reinterpret_cast<const char*>(&hasBoneList), sizeof(hasBoneList));
            }
            
            // --- Add camera for players ---
            if (isPlayer && m_RecordPlayerCameras) {
                dictWriter.WriteString("camera");
                bool thirdPerson = true;
                file->write(reinterpret_cast<const char*>(&thirdPerson), sizeof(thirdPerson));
                
                // Player position with eye offset
                float playerPos[3];
                playerPos[0] = pos[0];
                playerPos[1] = pos[1];
                playerPos[2] = pos[2] + 64.0f; // Eye offset
                
                file->write(reinterpret_cast<const char*>(&playerPos[0]), sizeof(float));
                file->write(reinterpret_cast<const char*>(&playerPos[1]), sizeof(float));
                file->write(reinterpret_cast<const char*>(&playerPos[2]), sizeof(float));
                
                // Player angles
                file->write(reinterpret_cast<const char*>(&ang[0]), sizeof(float));
                file->write(reinterpret_cast<const char*>(&ang[1]), sizeof(float));
                file->write(reinterpret_cast<const char*>(&ang[2]), sizeof(float));
                
                // Default FOV
                float playerFov = 90.0f;
                // Explicitly ensure it's a valid FOV to avoid division by zero
                if (playerFov <= 0.01f || playerFov >= 179.9f) {
                    playerFov = 90.0f;
                }
                file->write(reinterpret_cast<const char*>(&playerFov), sizeof(playerFov));
            }
        }
    }
    
    // End frame
    dictWriter.WriteString("afxFrameEnd");
}

void MirvAgr::WriteHeader() {
    if (!m_File) return;
    const char magic[] = "afxGameRecord\0";
    static_cast<std::ofstream*>(m_File)->write(magic, 14);
    int32_t version = 5;
    static_cast<std::ofstream*>(m_File)->write(reinterpret_cast<const char*>(&version), sizeof(version));
}

void MirvAgr::WriteFrame() {
    // Not used, handled in CaptureFrame
}

void MirvAgr::WriteFooter() {
    if (!m_File) return;
    // No special footer needed for AGR format
}

void MirvAgr::SetRecordCamera(bool enable) { m_RecordCamera = enable; }
bool MirvAgr::GetRecordCamera() const { return m_RecordCamera; }
void MirvAgr::SetRecordPlayers(bool enable) { m_RecordPlayers = enable; }
bool MirvAgr::GetRecordPlayers() const { return m_RecordPlayers; }
void MirvAgr::SetRecordWeapons(bool enable) { m_RecordWeapons = enable; }
bool MirvAgr::GetRecordWeapons() const { return m_RecordWeapons; }
void MirvAgr::SetRecordProjectiles(bool enable) { m_RecordProjectiles = enable; }
bool MirvAgr::GetRecordProjectiles() const { return m_RecordProjectiles; }
void MirvAgr::SetRecordViewmodel(bool enable) { m_RecordViewmodel = enable; }
bool MirvAgr::GetRecordViewmodel() const { return m_RecordViewmodel; }
void MirvAgr::SetRecordPlayerCameras(bool enable) { m_RecordPlayerCameras = enable; }
bool MirvAgr::GetRecordPlayerCameras() const { return m_RecordPlayerCameras; }
void MirvAgr::SetRecordViewmodels(bool enable) { m_RecordViewmodels = enable; }
bool MirvAgr::GetRecordViewmodels() const { return m_RecordViewmodels; }

void MirvAgr::HandleConsoleCommand(advancedfx::ICommandArgs* args) {
    int argc = args->ArgC();
    if (argc < 2) {
        advancedfx::Message(
            "mirv_agr start <filename> | stop\n"
            "mirv_agr recordCamera 0|1\n"
            "mirv_agr recordPlayers 0|1\n"
            "mirv_agr recordWeapons 0|1\n"
            "mirv_agr recordProjectiles 0|1\n"
            "mirv_agr recordViewmodel 0|1\n"
            "mirv_agr recordPlayerCameras 0|1\n"
            "mirv_agr recordViewmodels 0|1\n"
        );
        return;
    }
    const char* cmd = args->ArgV(1);
    if (0 == _stricmp(cmd, "start")) {
        if (argc < 3) {
            advancedfx::Message("mirv_agr start <filename>\n");
            return;
        }
        g_MirvAgr.Start(args->ArgV(2));
    } else if (0 == _stricmp(cmd, "stop")) {
        g_MirvAgr.Stop();
    } else if (0 == _stricmp(cmd, "recordCamera")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordCamera 0|1\n"); return; }
        g_MirvAgr.SetRecordCamera(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordPlayers")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordPlayers 0|1\n"); return; }
        g_MirvAgr.SetRecordPlayers(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordWeapons")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordWeapons 0|1\n"); return; }
        g_MirvAgr.SetRecordWeapons(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordProjectiles")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordProjectiles 0|1\n"); return; }
        g_MirvAgr.SetRecordProjectiles(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordViewmodel")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordViewmodel 0|1\n"); return; }
        g_MirvAgr.SetRecordViewmodel(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordPlayerCameras")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordPlayerCameras 0|1\n"); return; }
        g_MirvAgr.SetRecordPlayerCameras(0 != atoi(args->ArgV(2)));
    } else if (0 == _stricmp(cmd, "recordViewmodels")) {
        if (argc < 3) { advancedfx::Message("mirv_agr recordViewmodels 0|1\n"); return; }
        g_MirvAgr.SetRecordViewmodels(0 != atoi(args->ArgV(2)));
    } else {
        advancedfx::Message("mirv_agr start <filename> | stop\n");
    }
}

CON_COMMAND(mirv_agr, "AFX GameRecord (CS2)") {
    g_MirvAgr.HandleConsoleCommand(args);
}

void RegisterMirvAgrCommand() {/* nothing needed, CON_COMMAND auto-registers */} 