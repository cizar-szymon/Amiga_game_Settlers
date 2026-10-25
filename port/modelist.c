/* modelist - prints the Picasso96 screen modes (id, size, depth, formats)
   and the mode p96BestModeIDTags() picks for a size.
   usage: modelist [width height] */
#include <exec/types.h>
#include <exec/lists.h>
#include <libraries/Picasso96.h>
#include <proto/exec.h>
#include <proto/Picasso96.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *P96Base;

static const char ver[] = "$VER: modelist 0.1 (25.10.2026)";

int main(int argc, char **argv)
{
  struct List *ml;
  struct P96Mode *m;
  ULONG w = 640, h = 480, id;

  (void)ver;
  if (argc >= 3) {
    w = strtoul(argv[1], NULL, 10);
    h = strtoul(argv[2], NULL, 10);
  }
  P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2);
  if (P96Base == NULL) {
    printf("no Picasso96API.library\n");
    return 10;
  }
  ml = p96AllocModeListTags(TAG_DONE);
  if (ml != NULL) {
    for (m = (struct P96Mode *)ml->lh_Head; m->Node.ln_Succ != NULL;
         m = (struct P96Mode *)m->Node.ln_Succ) {
      printf("%08lx %4u x %4u depth %2u  %s\n", (unsigned long)m->DisplayID,
             (unsigned)m->Width, (unsigned)m->Height, (unsigned)m->Depth,
             m->Description);
    }
    p96FreeModeList(ml);
  }
  id = p96BestModeIDTags(P96BIDTAG_NominalWidth, w,
                         P96BIDTAG_NominalHeight, h,
                         P96BIDTAG_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
  printf("best CLUT for %lu x %lu: %08lx", (unsigned long)w,
         (unsigned long)h, (unsigned long)id);
  if (id != INVALID_ID) {
    printf("  %lu x %lu depth %lu",
           (unsigned long)p96GetModeIDAttr(id, P96IDA_WIDTH),
           (unsigned long)p96GetModeIDAttr(id, P96IDA_HEIGHT),
           (unsigned long)p96GetModeIDAttr(id, P96IDA_DEPTH));
  }
  printf("\n");
  CloseLibrary(P96Base);
  return 0;
}
