#ifndef _NOSCANS_H_
#define _NOSCANS_H_

#include <mm_comm_aio.h>
#include <nosc.h>

typedef struct NoscAnsContext
{
    // for ans process
    void *nosc;
    ns_prms_t mNoscInitParams;

    //for debug
    FILE *fin;
    FILE *fout;
}NoscAnsContext;
NoscAnsContext* ConstructNoscAnsContext();
void DestructNoscAnsContext(NoscAnsContext *pCtx);
int NoscAnsProcess(void *cookie, AUDIO_FRAME_S *pFrm, const AIO_ATTR_S *pAttr, BOOL bSuspendAns);

#endif  /* _NOSCANS_H_ */

