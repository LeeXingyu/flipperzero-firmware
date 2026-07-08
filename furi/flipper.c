#include "flipper.h"
#include <applications.h>
#include <furi.h>
#include <furi_hal_version.h>
#include <furi_hal_memory.h>
#include <furi_hal_rtc.h>

#include <FreeRTOS.h>
#include <string.h>

#define TAG "Flipper"

#define HEAP_CANARY_VALUE 0x8BADF00D

#ifndef FLIPPER_START_SERVICE_CLI_VCP
#define FLIPPER_START_SERVICE_CLI_VCP 1
#endif

#ifndef FLIPPER_START_SERVICE_BT
#define FLIPPER_START_SERVICE_BT 0
#endif

#ifndef FLIPPER_START_SERVICE_DIALOGS
#define FLIPPER_START_SERVICE_DIALOGS 1
#endif

#ifndef FLIPPER_START_SERVICE_DOLPHIN
#define FLIPPER_START_SERVICE_DOLPHIN 0
#endif

#ifndef FLIPPER_START_SERVICE_DESKTOP
#define FLIPPER_START_SERVICE_DESKTOP 0
#endif

#ifndef FLIPPER_START_SERVICE_GUI
#define FLIPPER_START_SERVICE_GUI 1
#endif

#ifndef FLIPPER_START_SERVICE_INPUT
#define FLIPPER_START_SERVICE_INPUT 1
#endif

#ifndef FLIPPER_START_SERVICE_LOADER
#define FLIPPER_START_SERVICE_LOADER 1
#endif

#ifndef FLIPPER_START_SERVICE_NOTIFICATION
#define FLIPPER_START_SERVICE_NOTIFICATION 1
#endif

#ifndef FLIPPER_START_SERVICE_POWER
#define FLIPPER_START_SERVICE_POWER 0
#endif

#ifndef FLIPPER_START_SERVICE_STORAGE
#define FLIPPER_START_SERVICE_STORAGE 1
#endif

static bool flipper_service_enabled(const char* name) {
    if(strcmp(name, "CliVcpSrv") == 0) return FLIPPER_START_SERVICE_CLI_VCP;
    if(strcmp(name, "BtSrv") == 0) return FLIPPER_START_SERVICE_BT;
    if(strcmp(name, "DialogsSrv") == 0) return FLIPPER_START_SERVICE_DIALOGS;
    if(strcmp(name, "DolphinSrv") == 0) return FLIPPER_START_SERVICE_DOLPHIN;
    if(strcmp(name, "DesktopSrv") == 0) return FLIPPER_START_SERVICE_DESKTOP;
    if(strcmp(name, "GuiSrv") == 0) return FLIPPER_START_SERVICE_GUI;
    if(strcmp(name, "InputSrv") == 0) return FLIPPER_START_SERVICE_INPUT;
    if(strcmp(name, "LoaderSrv") == 0) return FLIPPER_START_SERVICE_LOADER;
    if(strcmp(name, "NotificationSrv") == 0) return FLIPPER_START_SERVICE_NOTIFICATION;
    if(strcmp(name, "PowerSrv") == 0) return FLIPPER_START_SERVICE_POWER;
    if(strcmp(name, "StorageSrv") == 0) return FLIPPER_START_SERVICE_STORAGE;
    return true;
}

static void flipper_print_version(const char* target, const Version* version) {
    if(version) {
        FURI_LOG_I(
            TAG,
            "\r\n\t%s version:\t%s\r\n"
            "\tBuild date:\t\t%s\r\n"
            "\tGit Commit:\t\t%s (%s)%s\r\n"
            "\tGit Branch:\t\t%s",
            target,
            version_get_version(version),
            version_get_builddate(version),
            version_get_githash(version),
            version_get_gitbranchnum(version),
            version_get_dirty_flag(version) ? " (dirty)" : "",
            version_get_gitbranch(version));
    } else {
        FURI_LOG_I(TAG, "No build info for %s", target);
    }
}

void flipper_init(void) {
    flipper_print_version("Firmware", furi_hal_version_get_firmware_version());

    FURI_LOG_I(TAG, "Boot mode %d, starting services", furi_hal_rtc_get_boot_mode());

    for(size_t i = 0; i < FLIPPER_SERVICES_COUNT; i++) {
        if(!flipper_service_enabled(FLIPPER_SERVICES[i].name)) {
            FURI_LOG_D(TAG, "Skipping service %s", FLIPPER_SERVICES[i].name);
            continue;
        }

        FURI_LOG_D(TAG, "Starting service %s", FLIPPER_SERVICES[i].name);

        FuriThread* thread = furi_thread_alloc_service(
            FLIPPER_SERVICES[i].name,
            FLIPPER_SERVICES[i].stack_size,
            FLIPPER_SERVICES[i].app,
            NULL);
        furi_thread_set_appid(thread, FLIPPER_SERVICES[i].appid);

        furi_thread_start(thread);
    }

    FURI_LOG_I(TAG, "Startup complete");
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configIDLE_TASK_STACK_DEPTH);
    *stack_size = configIDLE_TASK_STACK_DEPTH;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configTIMER_TASK_STACK_DEPTH);
    *stack_size = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationGetRandomHeapCanary(portPOINTER_SIZE_TYPE* pxHeapCanary) {
    *pxHeapCanary = HEAP_CANARY_VALUE;
}
