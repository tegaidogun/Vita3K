#ifdef __ANDROID__

#include <util/android_driver.h>

#include <util/fs.h>
#include <util/log.h>

#include <SDL3/SDL_system.h>
#include <adrenotools/driver.h>
#include <dlfcn.h>
#include <jni.h>

#include <fstream>
#include <optional>

namespace {

std::string jni_string_to_utf8(JNIEnv *env, jstring str) {
    const char *utf = env->GetStringUTFChars(str, nullptr);
    if (!utf)
        return {};

    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    return result;
}

static jobject get_android_application(JNIEnv *env) {
    jclass activity_thread_class = env->FindClass("android/app/ActivityThread");
    if (!activity_thread_class)
        return nullptr;

    jmethodID current_application = env->GetStaticMethodID(activity_thread_class, "currentApplication", "()Landroid/app/Application;");
    if (!current_application)
        return nullptr;

    return env->CallStaticObjectMethod(activity_thread_class, current_application);
}

std::optional<fs::path> get_android_files_dir(JNIEnv *env) {
    if (!env)
        return std::nullopt;

    env->PushLocalFrame(8);

    jobject application = get_android_application(env);
    if (!application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android application while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    jclass context_class = env->GetObjectClass(application);
    jmethodID get_files_dir = env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");
    if (!get_files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find Context.getFilesDir while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    jobject files_dir = env->CallObjectMethod(application, get_files_dir);
    if (!files_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android files dir for Vulkan driver probing");
        return std::nullopt;
    }

    jclass file_class = env->GetObjectClass(files_dir);
    jmethodID get_absolute_path = env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find File.getAbsolutePath while resolving Vulkan driver files dir");
        return std::nullopt;
    }

    auto *files_dir_str = reinterpret_cast<jstring>(env->CallObjectMethod(files_dir, get_absolute_path));
    if (!files_dir_str) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android files dir string for Vulkan driver probing");
        return std::nullopt;
    }

    const fs::path resolved = fs::path(jni_string_to_utf8(env, files_dir_str)) / "";
    env->PopLocalFrame(nullptr);
    return resolved;
}

std::optional<fs::path> get_android_native_library_dir(JNIEnv *env) {
    if (!env)
        return std::nullopt;

    env->PushLocalFrame(8);

    jobject application = get_android_application(env);
    if (!application) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve Android application while resolving native library dir");
        return std::nullopt;
    }

    jclass context_class = env->GetObjectClass(application);
    jmethodID get_application_info = env->GetMethodID(context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    if (!get_application_info) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find Context.getApplicationInfo while resolving native library dir");
        return std::nullopt;
    }

    jobject app_info = env->CallObjectMethod(application, get_application_info);
    if (!app_info) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve ApplicationInfo while resolving native library dir");
        return std::nullopt;
    }

    jclass app_info_class = env->GetObjectClass(app_info);
    jfieldID native_library_dir_field = env->GetFieldID(app_info_class, "nativeLibraryDir", "Ljava/lang/String;");
    if (!native_library_dir_field) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not find ApplicationInfo.nativeLibraryDir");
        return std::nullopt;
    }

    auto *native_library_dir = reinterpret_cast<jstring>(env->GetObjectField(app_info, native_library_dir_field));
    if (!native_library_dir) {
        env->PopLocalFrame(nullptr);
        LOG_ERROR("Could not retrieve ApplicationInfo.nativeLibraryDir");
        return std::nullopt;
    }

    const fs::path resolved = fs::path(jni_string_to_utf8(env, native_library_dir)) / "";
    env->PopLocalFrame(nullptr);
    return resolved;
}

} // namespace

namespace android_driver {

void *load_custom_vulkan_driver(const std::string &driver_name) {
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    const auto files_dir = get_android_files_dir(env);
    if (!files_dir)
        return nullptr;

    const fs::path driver_path = *files_dir / "driver" / driver_name / "";
    if (!fs::exists(driver_path)) {
        LOG_ERROR("Could not find driver {}", driver_name);
        return nullptr;
    }

    const fs::path driver_name_file = driver_path / "driver_name.txt";
    if (!fs::exists(driver_name_file)) {
        LOG_ERROR("Could not find driver driver_name.txt");
        return nullptr;
    }

    std::string main_so_name;
    std::ifstream name_file(fs_utils::path_to_utf8(driver_name_file), std::ios_base::in);
    name_file >> main_so_name;
    name_file.close();

    const char *temp_dir = nullptr;
    fs::path temp_dir_path;
    if (SDL_GetAndroidSDKVersion() < 29) {
        temp_dir_path = driver_path / "tmp/";
        fs::create_directory(temp_dir_path);
        temp_dir = temp_dir_path.c_str();
    }

    const auto lib_dir = get_android_native_library_dir(env);
    if (!lib_dir)
        return nullptr;

    fs::create_directory(driver_path / "file_redirect");

    void *vulkan_handle = adrenotools_open_libvulkan(
        RTLD_NOW,
        ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM,
        temp_dir,
        lib_dir->c_str(),
        driver_path.c_str(),
        main_so_name.c_str(),
        (driver_path / "file_redirect/").c_str(),
        nullptr);

    if (!vulkan_handle) {
        LOG_ERROR("Could not open handle for custom driver {}", driver_name);
        return nullptr;
    }

    return vulkan_handle;
}

} // namespace android_driver

#endif
