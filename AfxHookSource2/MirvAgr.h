#pragma once

#include "../shared/AfxConsole.h"
#include <cstdint>

// --- AGR Binary Structs (CS2, CSGO-style) ---
#pragma pack(push, 1)
struct afxGameRecord {
    char magic[8]; // "HLAGR\0\0"
    int32_t version; // e.g. 5
    int32_t tickrate; // e.g. 64
    int32_t reserved[13]; // pad to 64 bytes
};

struct afxFrame {
    int32_t tick;
    float time;
    int32_t entityCount;
    int32_t reserved[5]; // pad to 32 bytes
};

struct entity_state {
    int32_t entityIndex;
    int32_t classId;
    float origin[3];
    float angles[3];
    int32_t health;
    int32_t team;
    // baseentity/baseanimating fields (add more as needed)
    int32_t animSequence;
    float animCycle;
    int32_t reserved[8]; // pad to 64 bytes
};
#pragma pack(pop)

// Handles AFX GameRecord (AGR) recording for CS2.
class MirvAgr {
public:
    MirvAgr();
    ~MirvAgr();

    void Start(const char* filename);
    void Stop();
    bool IsRecording() const;
    void CaptureFrame(); // Call this per frame/tick to record state

    void SetRecordCamera(bool enable);   bool GetRecordCamera() const;
    void SetRecordPlayers(bool enable);  bool GetRecordPlayers() const;
    void SetRecordWeapons(bool enable);  bool GetRecordWeapons() const;
    void SetRecordProjectiles(bool enable); bool GetRecordProjectiles() const;
    void SetRecordViewmodel(bool enable); bool GetRecordViewmodel() const;
    void SetRecordPlayerCameras(bool enable); bool GetRecordPlayerCameras() const;
    void SetRecordViewmodels(bool enable); bool GetRecordViewmodels() const;

    void HandleConsoleCommand(advancedfx::ICommandArgs* args);

private:
    void WriteHeader();
    void WriteFrame();
    void WriteFooter();
    // Add members for file handle, state, etc.
    void* m_File = nullptr;
    bool m_Recording = false;
    bool m_RecordCamera = true;
    bool m_RecordPlayers = true;
    bool m_RecordWeapons = true;
    bool m_RecordProjectiles = true;
    bool m_RecordViewmodel = true;
    bool m_RecordPlayerCameras = false;
    bool m_RecordViewmodels = false;
};

void RegisterMirvAgrCommand(); 

extern MirvAgr g_MirvAgr; 