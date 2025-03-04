#include "xms.h"
#include <stdlib.h>

#pragma pack(1)

/* XMS memory copy structure */
typedef struct {
  uint32_t copy_len;               /* must be EVEN */
  uint16_t srce_hndle;              /* source handle */
  uint32_t srce_ofs;               /* offset in source block */
  uint16_t dest_hndle;              /* dest handle */
  uint32_t dest_ofs;               /* offset in dest block */
} XMSCOPY, *XMSCOPY_PTR;

typedef int (far *FARPROC)(void);
FARPROC xms_entrypoint = NULL;  /* obtained from Int 2fh/4310h */

/* ------- XMS functions ------------------------------- */

/* if XMS present, store entry point in xms_entrypoint */
int xms_is_present(void)
{
  int available;

  if (xms_entrypoint)
    return TRUE;

  available = FALSE;

  _asm {
    mov ax, 0x4300;
    int 0x2f;
    cmp al, 0x80;
    je present;
    jmp done;

 present:
    mov available, TRUE;

    mov ax, 0x4310;
    int 0x2f;
    push ds;
    mov ax, seg xms_entrypoint;
    mov ds, ax;
    mov word ptr xms_entrypoint, bx;
    mov word ptr xms_entrypoint+2, es;
    pop ds;

  done:
  }

  return available;
}

/* Return size of largest free block. Return 0 if error
        Ignore the 'No return value' compiler warning for this function. */
uint16_t xms_kb_avail(void)
{
  _asm {
    mov ah, 0x08;
  }
  return xms_entrypoint();
}

/* Allocate a chunk of XMS and return a handle */
int xms_alloc_block(uint16_t block_size, uint16_t *handle_ptr)
{
  int success = FALSE;
  uint16_t handle, result;

  _asm {
    mov ah, 0x09;
    mov dx, block_size;
  }
  result = xms_entrypoint();
  _asm {
    mov handle, dx;
  }
  if (result) {
    *handle_ptr = handle;
    success = TRUE;
  }

  return success;
}

/* free XMS memory previously allocated */
int xms_free_block(uint16_t handle)
{
  int success = FALSE;

  _asm {
    push ds;
    mov ax, seg xms_entrypoint;
    mov ds, ax;
    mov ah, 0x0A;
    mov dx, handle;

    call dword ptr xms_entrypoint;
    pop ds;
    cmp ax, 0x0000;
    je done;
    mov success, 1
  done:
  }

  return success;
}

// Only used by xms_copy() but doesn't work as stack/local var
static XMSCOPY xms;

int xms_copy(uint16_t source, uint32_t source_offset, uint16_t dest, uint32_t dest_offset, uint32_t length)
{
  uint16_t result;
  uint8_t err;
  void *xms_ptr = &xms;


  xms.copy_len = length;
  xms.srce_hndle = source;
  xms.srce_ofs = source_offset;
  xms.dest_hndle = dest;
  xms.dest_ofs = dest_offset;

  _asm {
    push ds;
    push si;
    mov si, xms_ptr;
    mov ah, 0x0b;
    call dword ptr [xms_entrypoint];
    pop si;
    pop ds;
    mov result, ax;
    mov err, bl;
  }

  return result == 1;
}
