/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * JNI innards, common to the regular and "checked" interfaces.
 */
#ifndef _DALVIK_JNIINTERNAL
#define _DALVIK_JNIINTERNAL

#include "jni.h"

/* system init/shutdown */
bool dvmJniStartup(void);
void dvmJniShutdown(void);

/*
 * Our data structures for JNIEnv and JavaVM.
 *
 * Native code thinks it has a pointer to a pointer.  We know better.
 */
struct JavaVMExt;

typedef struct JNIEnvExt {
    const struct JNINativeInterface* funcTable;     /* must be first */

    const struct JNINativeInterface* baseFuncTable;

    /* pointer to the VM we are a part of */
    struct JavaVMExt* vm;

    u4      envThreadId;
    Thread* self;

    /* if nonzero, we are in a "critical" JNI call */
    int     critical;

    /* keep a copy of this here for speed */
    bool    forceDataCopy;

    struct JNIEnvExt* prev;
    struct JNIEnvExt* next;
} JNIEnvExt;

typedef struct JavaVMExt {
    const struct JNIInvokeInterface* funcTable;     /* must be first */

    const struct JNIInvokeInterface* baseFuncTable;

    /* if multiple VMs are desired, add doubly-linked list stuff here */

    /* per-VM feature flags */
    bool    useChecked;
    bool    warnError;
    bool    forceDataCopy;

    /* head of list of JNIEnvs associated with this VM */
    JNIEnvExt*      envList;
    pthread_mutex_t envListLock;
} JavaVMExt;

/*
 * Native function return type; used by dvmPlatformInvoke().
 *
 * This is part of Method.jniArgInfo, and must fit in 3 bits.
 * Note: Assembly code in arch/<arch>/Call<arch>.S relies on
 * the enum values defined here.
 */
typedef enum DalvikJniReturnType {
    DALVIK_JNI_RETURN_VOID = 0,     /* must be zero */
    DALVIK_JNI_RETURN_FLOAT = 1,
    DALVIK_JNI_RETURN_DOUBLE = 2,
    DALVIK_JNI_RETURN_S8 = 3,
    DALVIK_JNI_RETURN_S4 = 4,
    DALVIK_JNI_RETURN_S2 = 5,
    DALVIK_JNI_RETURN_U2 = 6,
    DALVIK_JNI_RETURN_S1 = 7
} DalvikJniReturnType;

#define DALVIK_JNI_NO_ARG_INFO  0x80000000
#define DALVIK_JNI_RETURN_MASK  0x70000000
#define DALVIK_JNI_RETURN_SHIFT 28
#define DALVIK_JNI_COUNT_MASK   0x0f000000
#define DALVIK_JNI_COUNT_SHIFT  24


/*
 * Pop the JNI local stack when we return from a native method.  "saveArea"
 * points to the StackSaveArea for the method we're leaving.
 *
 * (This may be implemented directly in assembly in mterp, so changes here
 * may only affect the portable interpreter.)
 */
INLINE void dvmPopJniLocals(Thread* self, StackSaveArea* saveArea)
{
#ifdef USE_INDIRECT_REF
    self->jniLocalRefTable.segmentState.all = saveArea->xtra.localRefCookie;
#else
    self->jniLocalRefTable.nextEntry = saveArea->xtra.localRefCookie;
#endif
}

/*
 * Set the envThreadId field.
 */
INLINE void dvmSetJniEnvThreadId(JNIEnv* pEnv, Thread* self)
{
    ((JNIEnvExt*)pEnv)->envThreadId = self->threadId;
    ((JNIEnvExt*)pEnv)->self = self;
}

/*
 * JNI call bridges.  Not called directly.
 *
 * The "Check" versions are used when CheckJNI is enabled.
 */
void dvmCallJNIMethod_general(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCallJNIMethod_synchronized(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCallJNIMethod_virtualNoRef(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCallJNIMethod_staticNoRef(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCheckCallJNIMethod_general(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCheckCallJNIMethod_synchronized(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCheckCallJNIMethod_virtualNoRef(const u4* args, JValue* pResult,
    const Method* method, Thread* self);
void dvmCheckCallJNIMethod_staticNoRef(const u4* args, JValue* pResult,
    const Method* method, Thread* self);

/*
 * Configure "method" to use the JNI bridge to call "func".
 */
void dvmUseJNIBridge(Method* method, void* func);


/*
 * Enable the "checked" versions.
 */
void dvmUseCheckedJniEnv(JNIEnvExt* pEnv);
void dvmUseCheckedJniVm(JavaVMExt* pVm);
void dvmLateEnableCheckedJni(void);

/*
 * Decode a local, global, or weak-global reference.
 */
#ifdef USE_INDIRECT_REF
Object* dvmDecodeIndirectRef(JNIEnv* env, jobject jobj);
#else
/* use an inline to ensure this is a no-op */
INLINE Object* dvmDecodeIndirectRef(JNIEnv* env, jobject jobj) {
    return (Object*) jobj;
}
#endif

/*
 * Verify that a reference passed in from native code is valid.  Returns
 * an indication of local/global/invalid.
 */
jobjectRefType dvmGetJNIRefType(JNIEnv* env, jobject jobj);

/*
 * Get the last method called on the interp stack.  This is the method
 * "responsible" for calling into JNI.
 */
const Method* dvmGetCurrentJNIMethod(void);

/*
 * Create/destroy a JNIEnv for the current thread.
 */
JNIEnv* dvmCreateJNIEnv(Thread* self);
void dvmDestroyJNIEnv(JNIEnv* env);

/*
 * Find the JNIEnv associated with the current thread.
 */
JNIEnvExt* dvmGetJNIEnvForThread(void);

/*
 * Extract the return type enum from the "jniArgInfo" value.
 */
DalvikJniReturnType dvmGetArgInfoReturnType(int jniArgInfo);

/*
 * Release all MonitorEnter-acquired locks that are still held.  Called at
 * DetachCurrentThread time.
 */
void dvmReleaseJniMonitors(Thread* self);

#endif /*_DALVIK_JNIINTERNAL*/
