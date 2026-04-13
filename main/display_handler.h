#ifndef __ESPM_DISPLAY_HANDLER_H__
#define __ESPM_DISPLAY_HANDLER_H__


#define DISPLAY_SIZE_WIDTH 1024
#define DISPLAY_SIZE_HEIGHT 600
/* Lazy calc on 14px mono font on 1024x600 display */
#define DISPLAY_MAX_CHAR_PER_ROW 73
#define DISPLAY_MAX_CHAR_ROWS 42

typedef struct 
{

} DH;

/* ======================= INTERFACE ======================= */

int display_handler_init(DH* _DH);

void display_handler_work(void* _null_for_now);

// void dh_dispose(DH* _DH);

/* ========================================================= */
#endif
