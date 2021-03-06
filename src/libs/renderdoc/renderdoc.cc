// -*- mode: c++; -*-

#include <dlfcn.h>

#include "renderdoc.hh"
#include "renderdoc_app.hh"
#include "shared/types.hh"

namespace {
bool api_loaded = false;
RENDERDOC_API_1_1_1* api = nullptr;

pRENDERDOC_GetAPI load_getAPI () {
    auto handle = dlopen ("librenderdoc.so", RTLD_NOLOAD);
    auto symbol = dlsym (handle, "RENDERDOC_GetAPI");

    if (handle != nullptr) { dlclose (handle); }

    return (pRENDERDOC_GetAPI)symbol;
}

} // namespace

namespace renderdoc {

bool loadApi () {
    if (api_loaded) { return true; }

    auto getAPI = load_getAPI ();

    if (getAPI == nullptr) { return false; }

    auto res = getAPI (
        eRENDERDOC_API_Version_1_1_1, reinterpret_cast< void** > (&api));

    if (res != 1) { return false; }

    api_loaded = true;

    return true;
}

void triggerCapture () {
    if (!api_loaded) {
        // Do nothing if API is not available
        return;
    }

    api->TriggerCapture ();
}
void startCapture () {
    if (api_loaded) { api->StartFrameCapture (nullptr, nullptr); }
}
bool isCapturing () {
    if (api_loaded) { return api->IsFrameCapturing () == 1; }

    return false;
}
void endCapture () {
    if (api_loaded) { api->EndFrameCapture (nullptr, nullptr); }
}

} // namespace renderdoc
