#include "al/Library/Base/String.h"
#include "al/Library/LiveActor/ActorInitInfo.h"
#include "al/Library/LiveActor/LiveActor.h"
#include "al/Library/Placement/PlacementInfo.h"
#include "al/Library/Player/PlayerHolder.h"
#include "al/Project/HitSensor/HitSensor.h"
#include "diag/assert.hpp"
#include "fs/fs_files.hpp"
#include "fs/fs_mount.hpp"
#include "fs/fs_types.hpp"
#include "game/Interfaces/IUsePlayerHack.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "helpers/createActorByNameHelper.h"
#include "helpers/fsHelper.h"
#include "hook/trampoline.hpp"
#include "init.h"
#include "lib.hpp"
#include "patches.hpp"
#include "ExceptionHandler.h"
#include "helpers.h"
#include "game/Player/PlayerHackKeeper.h"

#include <basis/seadRawPrint.h>
#include <prim/seadSafeString.h>
#include <resource/seadResourceMgr.h>
#include <filedevice/nin/seadNinSDFileDeviceNin.h>
#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <resource/seadArchiveRes.h>
#include <heap/seadHeapMgr.h>
#include <devenv/seadDebugFontMgrNvn.h>
#include <gfx/seadTextWriter.h>
#include <gfx/seadViewport.h>

#include <al/Library/File/FileLoader.h>
#include <al/Library/File/FileUtil.h>

#include <game/StageScene/StageScene.h>
#include <game/System/GameSystem.h>
#include <game/System/Application.h>
#include <game/HakoniwaSequence/HakoniwaSequence.h>
#include <game/GameData/GameDataFunction.h>

#include <al/Library/Scene/SceneUtil.h>
#include <al/Library/LiveActor/ActorPoseKeeper.h>
#include "rs/util.hpp"
#include "al/Library/Math/MathRandomUtil.h"
#include <al/Library/HitSensor/HitSensorKeeper.h>
#include <al/Library/Controller/JoyPadUtil.h>
#include <al/Library/Yaml/Writer/ByamlWriter.h>
#include <al/Library/Memory/HeapUtil.h>
#include "CustomWriteStream.h"
#include <sead/prim/seadSafeString.h>
#include <string>


al::HitSensor* getLockOnSensor(const al::LiveActor*);

#define ENEMY_RATE 1800
#define WAIT_AFTER_RELOAD 180

const char* enemyNamesFile;
int enemyNamesFileLenght = 0;
int enemyCount = 0;
al::LiveActor* enemies[30];

int frameCounter = 0;

void createActor(const char* name, const al::PlacementInfo* placement, al::ActorInitInfo* info, al::ActorFactory const* factory, int index){
    Logger::log("\n\nGot name: %s", name);
    if(strlen(name) < 2)
        return;

auto creator = factory->getCreator(name);
                if(!creator){
                   EXL_ABORT(0, "Could not create of type %s! Are you sure it exists?", name);
                }
            enemies[index] = creator(name);
            sead::FormatFixedSafeString<60> actorInfoPath("sd:/smo/RandomCaptureMod/ActorInfo/%s.byml", name);
            if(FsHelper::isFileExist(actorInfoPath.cstr())){
            auto customInfo = *placement;
            auto loadData = FsHelper::LoadData{
                .path = actorInfoPath.cstr()
            };
            FsHelper::loadFileFromPath(loadData);

            
            customInfo.mPlacementIter = al::ByamlIter((u8*) loadData.buffer);
            al::initCreateActorWithPlacementInfo(enemies[index], *info, customInfo);
        }
        else{
            al::initCreateActorNoPlacementInfo(enemies[index], *info);
    }
            enemies[index]->makeActorDead();
}

HOOK_DEFINE_TRAMPOLINE(SceneInit){
    static void Callback(al::ActorInitInfo* info, StageScene* curScene, al::PlacementInfo const* placement, al::LayoutInitInfo const* lytInfo,
                                                 al::ActorFactory const* factory, al::SceneMsgCtrl* sceneMsgCtrl, al::GameDataHolderBase* dataHolder) {

        al::initActorInitInfo(info, curScene, placement, lytInfo, factory, sceneMsgCtrl, dataHolder);
            frameCounter = ENEMY_RATE - WAIT_AFTER_RELOAD;
            int bufIndex = 0;   
            char* buff = (char*) al::getSceneHeap()->alloc(sizeof(char)*50);
            for(int i = 0; i < enemyNamesFileLenght; i++){
                if(enemyNamesFile[i] != '\n'){
                    buff[bufIndex] = enemyNamesFile[i];
                    bufIndex++;
                    continue;
                }
                buff[bufIndex] = '\0';
                bufIndex = 0;
                createActor(buff, placement, info, factory, enemyCount);
                buff = (char*) al::getSceneHeap()->alloc(sizeof(char)*50);
                enemyCount++;
        }
        
        Orig(info, curScene, placement, lytInfo, factory, sceneMsgCtrl, dataHolder);
    }
};


HOOK_DEFINE_TRAMPOLINE(GameSystemInit){
    static void Callback(void* thisPtr){
        auto loadData = FsHelper::LoadData{
            .path = "sd:/smo/RandomCaptureMod/actors.txt"
        };
        FsHelper::loadFileFromPath(loadData);
        if(!loadData.bufSize){
            EXL_ABORT(0, "Could not load actors.txt! Are you sure it exists?");
        }
        enemyNamesFile = (const char*) loadData.buffer;
        enemyNamesFileLenght = loadData.bufSize;   
        Logger::log("File data:%s", loadData.buffer);
        Orig(thisPtr);
    }
};

HOOK_DEFINE_TRAMPOLINE(ControlHook) {
        static void Callback(StageScene *scene) {
        if(scene->mIsAlive && !scene->isPause()) {
            
            auto player = (PlayerActorHakoniwa*) rs::getPlayerActor(scene);
            int enemyIndex = al::getRandom(0, enemyCount);
            if (frameCounter % ENEMY_RATE == 0 || (al::isPadHoldL(-1) && al::isPadTriggerLeft(-1))) {
                
                while (!enemies[enemyIndex] || (player && al::getAlivePlayerNum(player) > 0 && player->mHackKeeper && player->mHackKeeper->currentHackActor && al::isEqualString(player->mHackKeeper->currentHackActor->mActorName, enemies[enemyIndex]->mActorName))) {
                    enemyIndex = al::getRandom(0, enemyCount);
       
                }

                if(player && player->mHackKeeper && player->mHackKeeper->currentHackActor){
                    player->mHackKeeper->cancelHack();
                }

                auto playerTrans = al::getTrans(player);
                auto enemyTrans = playerTrans;
                enemyTrans.y += 200;
//                enemyTrans.x += 150;
                enemies[enemyIndex]->makeActorAlive();
                al::setTrans(enemies[enemyIndex], enemyTrans);
                enemies[enemyIndex]->appear();
              /* 
                if(player && player->mHackCap && getLockOnSensor(enemies[enemyIndex])){
                    auto sensor = getLockOnSensor(enemies[enemyIndex]);
                    player->mHackCap->prepareLockOn(enemies[enemyIndex]->mHitSensorKeeper->mSensors[0]);
                }
                */

                if(player && player->mHackCap && enemies[enemyIndex] && enemies[enemyIndex]->mHitSensorKeeper && enemies[enemyIndex]->mHitSensorKeeper && enemies[enemyIndex]->mHitSensorKeeper->mSensors && enemies[enemyIndex]->mHitSensorKeeper->mSensorCount > 0 && al::isEqualString(enemies[enemyIndex]->mHitSensorKeeper->mSensors[0]->mName, "Body")){
                    player->mHackCap->prepareLockOn(enemies[enemyIndex]->mHitSensorKeeper->mSensors[0]);
                }
                
            }
            frameCounter++;
        }
        Orig(scene);
    }
};

HOOK_DEFINE_TRAMPOLINE(ZR_ZLInputHook){
    static bool Callback(int port){
        return Orig(port) && al::isPadHoldL(port);
    }
};

HOOK_DEFINE_TRAMPOLINE(CreateFileDeviceMgr) {
    static void Callback(sead::FileDeviceMgr *thisPtr) {

        Orig(thisPtr);

        thisPtr->mMountedSd = nn::fs::MountSdCardForDebug("sd").isSuccess();

        sead::NinSDFileDevice *sdFileDevice = new sead::NinSDFileDevice();

        thisPtr->mount(sdFileDevice);
    }
};

HOOK_DEFINE_TRAMPOLINE(RedirectFileDevice) {
    static sead::FileDevice *
    Callback(sead::FileDeviceMgr *thisPtr, sead::SafeString &path, sead::BufferedSafeString *pathNoDrive) {

        sead::FixedSafeString<32> driveName;
        sead::FileDevice *device;

        // Logger::log("Path: %s\n", path.cstr());

        if (!sead::Path::getDriveName(&driveName, path)) {

            device = thisPtr->findDevice("sd");

            if (!(device && device->isExistFile(path))) {

                device = thisPtr->getDefaultFileDevice();

                if (!device) {
                    Logger::log("drive name not found and default file device is null\n");
                    return nullptr;
                }

            } else {
                Logger::log("Found File on SD! Path: %s\n", path.cstr());
            }

        } else
            device = thisPtr->findDevice(driveName);

        if (!device)
            return nullptr;

        if (pathNoDrive != nullptr)
            sead::Path::getPathExceptDrive(pathNoDrive, path);

        return device;
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderLoadArc) {
    static sead::ArchiveRes *
    Callback(al::FileLoader *thisPtr, sead::SafeString &path, const char *ext, sead::FileDevice *device) {

        // Logger::log("Path: %s\n", path.cstr());

        sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

        if (sdFileDevice && sdFileDevice->isExistFile(path)) {

            Logger::log("Found File on SD! Path: %s\n", path.cstr());

            device = sdFileDevice;
        }

        return Orig(thisPtr, path, ext, device);
    }
};

sead::FileDevice *tryFindNewDevice(sead::SafeString &path, sead::FileDevice *orig) {
    sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

    if (sdFileDevice && sdFileDevice->isExistFile(path))
        return sdFileDevice;

    return orig;
}

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistFile) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistArchive) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};


extern "C" void exl_main(void *x0, void *x1) {
    /* Setup hooking enviroment. */
    exl::hook::Initialize();

    nn::os::SetUserExceptionHandler(exception_handler, nullptr, 0, nullptr);
    installExceptionStub();

   
    runCodePatches();

    // SD File Redirection
    Logger::instance().init("0", 0);

    RedirectFileDevice::InstallAtOffset(0x76CFE0);
    FileLoaderLoadArc::InstallAtOffset(0xA5EF64);
    CreateFileDeviceMgr::InstallAtOffset(0x76C8D4);
    FileLoaderIsExistFile::InstallAtSymbol(
            "_ZNK2al10FileLoader11isExistFileERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");
    FileLoaderIsExistArchive::InstallAtSymbol(
            "_ZNK2al10FileLoader14isExistArchiveERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");
    
    SceneInit::InstallAtOffset(0x4C8DD0);
    GameSystemInit::InstallAtSymbol("_ZN10GameSystem4initEv");
    ControlHook::InstallAtSymbol("_ZN10StageScene7controlEv");
    ZR_ZLInputHook::InstallAtSymbol("_ZN2al14isPadTriggerZREi");
    ZR_ZLInputHook::InstallAtSymbol("_ZN2al14isPadTriggerZLEi");

}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}
