// Loaded libraries own the returned counter and its code for the worker's lifetime.
#include "window_variants.h"
#include <dlfcn.h>
#include <map>
#include <sstream>

inline const benchmark::WindowVariant *load_tuning_counter(std::istringstream &fields)
{
    std::string library, counter;
    if (!std::getline(fields, library, '\t') || !std::getline(fields, counter, '\t')) {
        throw std::invalid_argument("expected a library path and counter name");
    }
    using Lookup = const benchmark::WindowVariant *(*)(const char *);
    static std::map<std::string, Lookup> loaded;
    auto &lookup = loaded[library];
    if (!lookup) {
        void *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            throw std::runtime_error(dlerror());
        }
        lookup = reinterpret_cast<Lookup>(dlsym(handle, "scrabble_window_counter"));
        if (!lookup) {
            throw std::runtime_error(dlerror());
        }
    }
    const auto *variant = lookup(counter.c_str());
    Dl_info info{};
    if (!dladdr(reinterpret_cast<const void *>(variant->solve), &info)) {
        throw std::runtime_error("could not identify loaded solve function");
    }
    std::printf("%s\t%s\n", info.dli_fname, variant->name);
    return variant;
}
