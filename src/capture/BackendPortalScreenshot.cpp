#include "capture/BackendPortalScreenshot.hpp"

#include <dbus/dbus.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#if __has_include(<stb_image.h>)
#include <stb_image.h>
#elif __has_include(<stb/stb_image.h>)
#include <stb/stb_image.h>
#else
#error "stb_image.h not found"
#endif

#include "platform/FileUtil.hpp"
#include "platform/Log.hpp"

namespace coomer {

namespace {

void appendDictEntry(DBusMessageIter* dict, const char* key, int type,
                     const void* value, const char* sig) {
    DBusMessageIter entry;
    DBusMessageIter variant;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, sig, &variant);
    dbus_message_iter_append_basic(&variant, type, value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

bool parseResponse(DBusMessage* msg, std::string& outUri) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(msg, &iter)) {
        return false;
    }
    uint32_t response = 1;
    dbus_message_iter_get_basic(&iter, &response);
    if (response != 0) {
        return false;
    }
    if (!dbus_message_iter_next(&iter)) {
        return false;
    }
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
        return false;
    }
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&iter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entryIter;
        dbus_message_iter_recurse(&arrayIter, &entryIter);
        const char* key = nullptr;
        dbus_message_iter_get_basic(&entryIter, &key);
        if (!dbus_message_iter_next(&entryIter)) {
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        if (dbus_message_iter_get_arg_type(&entryIter) == DBUS_TYPE_VARIANT) {
            DBusMessageIter varIter;
            dbus_message_iter_recurse(&entryIter, &varIter);
            if (key && std::strcmp(key, "uri") == 0 &&
                dbus_message_iter_get_arg_type(&varIter) == DBUS_TYPE_STRING) {
                const char* uri = nullptr;
                dbus_message_iter_get_basic(&varIter, &uri);
                if (uri) {
                    outUri = uri;
                    return true;
                }
            }
        }
        dbus_message_iter_next(&arrayIter);
    }
    return false;
}

}  // namespace

class PortalScreenshotBackend final : public ICaptureBackend {
public:
    std::string name() const override {
        return "portal-screenshot";
    }

    bool isAvailable() const override {
        DBusError err;
        dbus_error_init(&err);
        DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (!conn) {
            dbus_error_free(&err);
            return false;
        }
        bool hasOwner = dbus_bus_name_has_owner(
            conn, "org.freedesktop.portal.Desktop", &err);
        dbus_connection_unref(conn);
        if (dbus_error_is_set(&err)) {
            dbus_error_free(&err);
            return false;
        }
        return hasOwner;
    }

    std::vector<MonitorInfo> listMonitors() override {
        LOG_WARN(
            "portal: monitor enumeration is not available via Screenshot "
            "portal");
        return {};
    }

    CaptureResult captureOnce(std::optional<std::string> monitorHint) override {
        CaptureResult result;
        if (monitorHint) {
            LOG_WARN(
                "portal: monitor selection not supported; system dialog "
                "decides output");
        }

        DBusError err;
        dbus_error_init(&err);
        DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (!conn) {
            LOG_ERROR("portal: failed to connect to session bus: %s",
                      err.message ? err.message : "unknown");
            dbus_error_free(&err);
            return result;
        }

        // Send org.freedesktop.portal.Screenshot.Screenshot, then wait for the
        // Request::Response signal.
        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot", "Screenshot");
        if (!msg) {
            LOG_ERROR("portal: failed to create message");
            dbus_connection_unref(conn);
            return result;
        }

        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        const char* parent = "";
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent);

        DBusMessageIter dict;
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
        dbus_bool_t interactive = 0;
        appendDictEntry(&dict, "interactive", DBUS_TYPE_BOOLEAN, &interactive,
                        "b");
        std::string token =
            "coomer" +
            std::to_string(static_cast<unsigned long long>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        const char* tokenCStr = token.c_str();
        appendDictEntry(&dict, "handle_token", DBUS_TYPE_STRING, &tokenCStr,
                        "s");
        dbus_message_iter_close_container(&args, &dict);

        DBusMessage* reply =
            dbus_connection_send_with_reply_and_block(conn, msg, 5000, &err);
        dbus_message_unref(msg);

        if (!reply) {
            LOG_ERROR("portal: Screenshot call failed: %s",
                      err.message ? err.message : "unknown");
            dbus_error_free(&err);
            dbus_connection_unref(conn);
            return result;
        }

        const char* handle = nullptr;
        if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &handle,
                                   DBUS_TYPE_INVALID) ||
            !handle) {
            LOG_ERROR("portal: unexpected reply for Screenshot: %s",
                      err.message ? err.message : "unknown");
            dbus_error_free(&err);
            dbus_message_unref(reply);
            dbus_connection_unref(conn);
            return result;
        }
        dbus_message_unref(reply);

        std::string matchRule =
            std::string(
                "type='signal',interface='org.freedesktop.portal.Request',"
                "member='Response',path='") +
            handle + "'";
        dbus_bus_add_match(conn, matchRule.c_str(), &err);
        dbus_connection_flush(conn);
        if (dbus_error_is_set(&err)) {
            LOG_ERROR("portal: failed to add match: %s",
                      err.message ? err.message : "unknown");
            dbus_error_free(&err);
            dbus_connection_unref(conn);
            return result;
        }

        std::string uri;
        bool gotResponse = false;
        const auto start = std::chrono::steady_clock::now();
        while (!gotResponse) {
            dbus_connection_read_write(conn, 100);
            DBusMessage* signal = dbus_connection_pop_message(conn);
            if (signal) {
                if (dbus_message_is_signal(
                        signal, "org.freedesktop.portal.Request", "Response")) {
                    gotResponse = parseResponse(signal, uri);
                    dbus_message_unref(signal);
                    if (!gotResponse) {
                        LOG_ERROR("portal: screenshot cancelled or failed");
                        dbus_connection_unref(conn);
                        return result;
                    }
                    break;
                }
                dbus_message_unref(signal);
            }
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start)
                    .count() > 30) {
                LOG_ERROR("portal: timed out waiting for response");
                dbus_connection_unref(conn);
                return result;
            }
        }

        dbus_connection_unref(conn);

        std::string path = fileUrlToPath(uri);
        int w = 0;
        int h = 0;
        int n = 0;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 4);
        if (!data) {
            LOG_ERROR("portal: failed to load screenshot");
            std::remove(path.c_str());  // Clean up even if load failed
            return result;
        }
        (void)n;

        result.image.w = w;
        result.image.h = h;
        result.image.rgba.assign(
            data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
        stbi_image_free(data);

        // Delete the temporary file created by portal
        std::remove(path.c_str());

        return result;
    }
};

std::unique_ptr<ICaptureBackend> CreateBackendPortalScreenshot() {
    return std::make_unique<PortalScreenshotBackend>();
}

}  // namespace coomer
