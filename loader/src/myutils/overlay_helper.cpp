#include <utils/logger.h>
#include <malloc.h>
#include "libfat.h"
#include <iosuhax.h>
#include <wups.h>
#include <fat.h>
#include "common/retain_vars.h"
#include "overlay_helper.h"
#include <dynamic_libs/gx2_functions.h>

#ifdef __cplusplus
extern "C" {
#endif

u32 * getFromGX2Buffer(struct buffer_store store, u32 size){
    if(store.buffer != NULL){
        DEBUG_FUNCTION_LINE("We try to use the GX2 buffer. Needed space %08X (%d kb), available %08X (%d kb).\n",size,size/1024,store.buffer_size,store.buffer_size/1024);
        if(store.buffer_size >= size){
            return (u32*) store.buffer;
        }
    }
    return NULL;
}

void overlay_helper(wups_overlay_options_type_t screen, overlay_callback callback){
    if(callback == NULL) return;
    if(!OSIsHomeButtonMenuEnabled()) return; // This pauses the game. Make sure to only do it when the home button is allowed.

    //TODO: Make sure this actually pauses the game (Hook on GX2VSync?) . Currently only tested from VPADRead which also pauses the game.

    OSScreenInit();

    u32 * screenbuffer0 = NULL;
    u32 * screenbuffer1 = NULL;

    bool allocated_tv = false;
    bool allocated_drc = false;

    if(screen == WUPS_OVERLAY_DRC_ONLY){
        u32 screen_buf1_size = OSScreenGetBufferSizeEx(1);
        screenbuffer1 = getFromGX2Buffer(tv_store,screen_buf1_size);
        if(screenbuffer1 == NULL){
            DEBUG_FUNCTION_LINE("We need to try to allocate buffer for the DRC.\n");
            screenbuffer1 = (u32*)memalign(0x100, screen_buf1_size);
            if(screenbuffer1 != NULL){
                DEBUG_FUNCTION_LINE("Successfully allocated DRC buffer!.\n");
                allocated_drc = true;
            }
        }
    } else if(screen == WUPS_OVERLAY_TV_ONLY){
        u32 screen_buf0_size = OSScreenGetBufferSizeEx(0);
        screenbuffer0 = getFromGX2Buffer(tv_store,screen_buf0_size);

        if(screenbuffer0 == NULL){
            DEBUG_FUNCTION_LINE("We need to try to allocate buffer for the TV.\n");
            screenbuffer0 = (u32*)memalign(0x100, screen_buf0_size);
            if(screenbuffer0 != NULL){
                DEBUG_FUNCTION_LINE("Successfully allocated TV buffer!.\n");
                allocated_tv = true;
            }
        }
    } else if( screen == WUPS_OVERLAY_DRC_AND_TV || WUPS_OVERLAY_DRC_AND_TV_WITH_DRC_PRIO){
        // TV Buffer init.
        u32 screen_buf0_size = OSScreenGetBufferSizeEx(0);
        screenbuffer0 = getFromGX2Buffer(tv_store,screen_buf0_size);
        if(screenbuffer0 == NULL){
            DEBUG_FUNCTION_LINE("We need to try to allocate buffer for the TV.\n");
            screenbuffer0 = (u32*)memalign(0x100, screen_buf0_size);
            if(screenbuffer0 != NULL){
                DEBUG_FUNCTION_LINE("Successfully allocated TV buffer!.\n");
                allocated_tv = true;
            }
        }


        // DRC Buffer init.
        u32 screen_buf1_size = OSScreenGetBufferSizeEx(1);

        // Doesn't fit in the GX2 DRC buffer, we don't need to check it.
        DEBUG_FUNCTION_LINE("We need to try to allocate buffer for the DRC.\n");
        screenbuffer1 = (u32*)memalign(0x100, screen_buf1_size);
        if(screenbuffer1 != NULL){
            DEBUG_FUNCTION_LINE("Successfully allocated DRC buffer!.\n");
            allocated_drc = true;
        }

        if(screenbuffer1 == NULL){ // FAILED to allocate,
            if(screen == WUPS_OVERLAY_DRC_AND_TV_WITH_DRC_PRIO){
                DEBUG_FUNCTION_LINE("Successfully allocated DRC buffer!.\n");
                //but we can use the TV buffer from GX2.
                if(!allocated_tv){ // Make sure it's not allocated
                    screenbuffer0 = NULL; // Bye bye
                }
                screenbuffer1 = getFromGX2Buffer(tv_store,screen_buf1_size); // Use it! Hopefully this works.
            }
        }
    }

    if(screenbuffer0 == NULL && screenbuffer1 == NULL){
        goto error_exit;
    }


    if(screenbuffer0 != NULL){ OSScreenSetBufferEx(0, (void *)screenbuffer0); }
    if(screenbuffer1 != NULL){ OSScreenSetBufferEx(1, (void *)screenbuffer1); }

    if(screenbuffer0 != NULL){ OSScreenEnableEx(0, 1); }
    if(screenbuffer1 != NULL){ OSScreenEnableEx(1, 1); }

    // Clear screens
    if(screenbuffer0 != NULL){ OSScreenClearBufferEx(0, 0); }
    if(screenbuffer1 != NULL){ OSScreenClearBufferEx(1, 0); }

    // Flip buffers
    if(screenbuffer0 != NULL){ OSScreenFlipBuffersEx(0); }
    if(screenbuffer1 != NULL){ OSScreenFlipBuffersEx(1); }

    wups_overlay_options_type_t real_screen_type;

    if(screenbuffer0 != NULL){
        if(screenbuffer1 != NULL){
            real_screen_type = WUPS_OVERLAY_DRC_AND_TV;
        } else {
            real_screen_type = WUPS_OVERLAY_TV_ONLY;
        }
    } else if(screenbuffer1 != NULL){
        real_screen_type = WUPS_OVERLAY_DRC_ONLY;
    } else {
        real_screen_type = WUPS_OVERLAY_NONE;
    }

    // call the callback.
    if(real_screen_type != WUPS_OVERLAY_NONE){
        callback(real_screen_type);
    }

    if(tv_store.buffer != NULL){
        GX2SetTVBuffer(tv_store.buffer,tv_store.buffer_size,tv_store.mode,tv_store.surface_format,tv_store.buffering_mode);
    }

    if(drc_store.buffer != NULL){
        GX2SetDRCBuffer(drc_store.buffer,drc_store.buffer_size,drc_store.mode,drc_store.surface_format,drc_store.buffering_mode);
    }

error_exit:
    if(allocated_tv){
        free(screenbuffer0);
        screenbuffer0 = NULL;
    }

    if(allocated_drc){
        free(screenbuffer1);
        screenbuffer1 = NULL;
    }

    return;
}

#ifdef __cplusplus
}
#endif
