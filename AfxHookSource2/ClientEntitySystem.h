#pragma once

#include <cstdint>

#include <Windows.h>
#include "../deps/release/prop/cs2/sdk_src/public/entityhandle.h"

bool Hook_ClientEntitySystem( void* pEntityList, void * pFnGetHighestEntityIterator, void * pFnGetEntityFromIndex );

bool Hook_ClientEntitySystem2();

bool Hook_GetSplitScreenPlayer( void* pAddr);

class CAfxEntityInstanceRef;

struct EntityListIterator{
	int index = -1;

	bool IsValid() const {
		return 0 < index;
	}

    int GetIndex() const {
        return index;
    }
};

class CEntityInstance {
public:
    const char * GetName();

    const char * GetDebugName();

    const char * GetClassName();

    const char * GetClientClassName();

    bool IsPlayerPawn();

    SOURCESDK::CS2::CBaseHandle GetPlayerPawnHandle();

    bool IsPlayerController();

    SOURCESDK::CS2::CBaseHandle GetPlayerControllerHandle();

    unsigned int GetHealth();

    int GetTeam();
	
    /**
     * @remarks FLOAT_MAX if invalid
     */
    void GetOrigin(float & x, float & y, float & z);

    void GetRenderEyeOrigin(float outOrigin[3]);

    void GetRenderEyeAngles(float outAngles[3]);

    SOURCESDK::CS2::CBaseHandle GetViewEntityHandle();

    SOURCESDK::CS2::CBaseHandle GetActiveWeaponHandle();

    const char * GetPlayerName();

    uint64_t GetSteamId();

    const char * GetSanitizedPlayerName();

    uint8_t GetObserverMode();
    SOURCESDK::CS2::CBaseHandle GetObserverTarget();

    // Methods needed for AGR recording
    bool IsPlayer() { return IsPlayerPawn() || IsPlayerController(); }
    
    int GetBoneCount() { return 0; /* Stub implementation */ }
    
    void GetBoneMatrix(int boneIndex, float outMatrix[12]) { 
        // Stub implementation - identity matrix
        static const float identityMatrix[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
        memcpy(outMatrix, identityMatrix, sizeof(float) * 12);
    }
    
    int32_t GetOwnerHandle() { return -1; /* Stub implementation */ }
    
    const char* GetModelName() { return "unknown_model"; /* Stub implementation */ }
    
    int32_t GetSequence() { return 0; /* Stub implementation */ }
    
    float GetCycle() { return 0.0f; /* Stub implementation */ }
    
    bool IsActive() { return true; /* Stub implementation */ }
    
    void GetVelocity(float outVelocity[3]) {
        // Stub implementation - zero velocity
        outVelocity[0] = 0.0f;
        outVelocity[1] = 0.0f;
        outVelocity[2] = 0.0f;
    }
};

typedef EntityListIterator * (__fastcall * GetHighestEntityIterator_t)(void * entityList, EntityListIterator * it);
typedef void * (__fastcall * GetEntityFromIndex_t)(void * pEntityList, int index);

extern GetHighestEntityIterator_t  g_GetHighestEntityIterator;
extern GetEntityFromIndex_t g_GetEntityFromIndex;

extern void ** g_pEntityList;