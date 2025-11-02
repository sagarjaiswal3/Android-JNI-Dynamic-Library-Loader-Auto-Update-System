# Android JNI Dynamic Library Loader & Auto-Update System

![License](https://img.shields.io/badge/License-MIT-green.svg)
![Platform](https://img.shields.io/badge/Platform-Android_NDK-blue)
![Language](https://img.shields.io/badge/Language-C++17-lightgrey)

A native Android NDK / JNI system that downloads and dynamically loads `.so` libraries at runtime.  
It supports version checking, remote updates, secure storage, and app restart for seamless updates.  
Designed for applications that require dynamic payload loading, modular updates, or secure runtime patches — provided for educational and research purposes.

## ✅ Features

- ✔ Download `.so` libraries from a remote server  
- ✔ Version check using a `version.txt` file  
- ✔ Saves library inside app internal storage (`/data/data/<package>/files/`)  
- ✔ Secure runtime loading via `dlopen()`  
- ✔ Auto-restart app after update  
- ✔ Detects already loaded libraries using `RTLD_NOLOAD`  
- ✔ Runs automatically at startup using `__attribute__((constructor))`  
- ✔ JNI helpers for file access & HTTP streaming

---

## 📁 Project Structure

```
jni/
│── Encryption/ # Optional encryption utilities
│── Includes/ # Header files and helpers
│── Tools/ # Low-level helpers & wrappers
│── Android.mk # NDK build configuration
│── Application.mk # Compiler settings (arm64, optimization, etc.)
│── main.cpp # Core loader logic
```

## 🔧 How It Works

1. Loader starts automatically when the native library is loaded.  
2. It obtains the app internal storage path via `ActivityThread.currentApplication()` → `getFilesDir()`.  
3. Downloads a remote `version.txt` file and compares it to the locally stored version.  
4. If an update is needed:
   - Downloads the updated `.so` to internal storage.  
   - Writes the new version to `version.txt`.  
   - Triggers app restart so the app can load the new library.  
5. If up-to-date, it loads the existing `.so` using `dlopen()`.


## 🔧 Build & Integration (Android AIDE / Android Studio)

This project was developed using **Android AIDE** and the NDK with `Android.mk` / `Application.mk`. You can build either directly on-device with AIDE or with Android Studio using `ndk-build`.

### Using Android AIDE
1. Copy the `jni/` folder into your AIDE project where it expects native code.  
2. AIDE will use `Android.mk` / `Application.mk` to build the native library.

### Using Android Studio (ndk-build)
1. Place the `jni/` folder inside your Android app module: ```app/src/main/jni/```
2. Enable `ndk-build` in the module `build.gradle`:

```groovy
android {
    compileSdkVersion 33
    defaultConfig {
        applicationId "com.example.app"
        minSdkVersion 21
        targetSdkVersion 33

        externalNativeBuild {
            ndkBuild {
                // optional: set ABI filters
                abiFilters "arm64-v8a", "armeabi-v7a"
            }
        }
    }

    externalNativeBuild {
        ndkBuild {
            path "src/main/jni/Android.mk"
        }
    }
}
```
3. Ensure local.properties contains the NDK path: ```ndk.dir=/path/to/android-ndk```
4. Build with Android Studio → Build → Make Project or command line: ```./gradlew assembleDebug```

## 📓 Notes
- **If you prefer CMake, convert Android.mk into CMakeLists.txt (minor changes required).**
- **Application.mk can set APP_ABI, optimization flags, and STL. Adjust for your target ABIs.**


## ✅ Requirements

- Android NDK (r21 or newer recommended)
- C++17
- App internet permission if downloading from remote URL
- Works on ARM64

## 🔐 Security Notes

- `.so` stored securely inside app data directory
- Checks if library already loaded using `RTLD_NOLOAD`
- Supports obfuscation using `XorStr`, `oxorany`, etc.

## 📌 Example Use Cases

- Dynamic feature loading
- Hot updates without reinstalling APK
- Modular game hacks / payload injection
- A/B library testing system
- Secure native code deployment

## 📚 References / Credits

This project uses or was inspired by the following open-source resources:

- [oxorany — Simple string obfuscation for C/C++](https://github.com/llxiaoyuan/oxorany)


Special thanks to the open-source community for providing tools, examples, and research that helped make this project possible.

## ⚠️ Disclaimer & Acceptable Use

**This project is provided for educational and research purposes only.
It is not intended for malicious, illegal, or unauthorized activities. By using the code in this repository you agree to comply with all applicable laws and the terms of service of any software or services involved.
The author is not responsible for misuse of this code.**

## 📝 License

This project is licensed under the **MIT License**.

---

## 👤 Author

**Sagar Jaiswal**  
Feel free to fork, contribute, or open issues!


