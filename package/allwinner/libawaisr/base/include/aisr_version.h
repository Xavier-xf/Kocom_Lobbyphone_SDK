#ifndef AISR_VERSION_H
#define AISR_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define REPO_TAG ""
#define REPO_PATCH ""
#define REPO_BRANCH "aisr"
#define REPO_COMMIT "7ada23b6100752a851a9a27277d0e65a61ccc6ce"
#define REPO_DATE "Wed Jun 25 16:58:46 2025 +0800"
#define REPO_AUTHOR "huangyeshu"
#define REPO_CHANGE_ID "I2811b3f53a38edc97b01c94a73f6b7026f835be4"
#define RELEASE_AUTHOR "huangyeshu"

static inline void LogVersionInfo(void)
{
    awaisr_dbg("\n"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>> Awaisr Version Info <<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
         "tag   : %s\n"
         "branch: %s\n"
         "commit: %s\n"
         "date  : %s\n"
         "author: %s\n"
         "change-id : %s\n"
         "release_author : %s\n"
         "patch : %s\n"
         "----------------------------------------------------------------------\n",
         REPO_TAG, REPO_BRANCH, REPO_COMMIT, REPO_DATE, REPO_AUTHOR, REPO_CHANGE_ID, RELEASE_AUTHOR, REPO_PATCH);
}

#define TagVersionInfo() LogVersionInfo()

#ifdef __cplusplus
}
#endif

#endif

