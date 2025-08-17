#pragma once

#include <exiv2/exiv2.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

// -------------------------------------------------------------
// Config options
// -------------------------------------------------------------
struct Options {
    bool in_place = false;
    bool backup = true;
    bool recursive = false;
    fs::path out_dir;
};

// -------------------------------------------------------------
// Utility: get directory of the current executable
// -------------------------------------------------------------
static fs::path exe_dir() {
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    _NSGetExecutablePath(buf.data(), &size);
    return fs::weakly_canonical(fs::path(buf).parent_path());
#else
    return fs::current_path();
#endif
}

// Escape for shell command
static std::string shell_escape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// -------------------------------------------------------------
// File type helpers
// -------------------------------------------------------------
static bool is_image(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = std::tolower(c);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".heic";
}

static bool is_pdf(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = std::tolower(c);
    return ext == ".pdf";
}

// -------------------------------------------------------------
// Find bundled qpdf
// -------------------------------------------------------------
static std::string qpdf_path_cached;

static const std::string& find_qpdf() {
    if (!qpdf_path_cached.empty()) return qpdf_path_cached;

    fs::path candidate = exe_dir() / "bin" / "qpdf";
    if (fs::exists(candidate)) {
        qpdf_path_cached = candidate.string();
        return qpdf_path_cached;
    }
    if (std::system("command -v qpdf >/dev/null 2>&1") == 0) {
        qpdf_path_cached = "qpdf";
        return qpdf_path_cached;
    }
    qpdf_path_cached.clear();
    return qpdf_path_cached;
}

// -------------------------------------------------------------
// Output path logic
// -------------------------------------------------------------
static fs::path default_output(const fs::path& in, const Options& opt) {
    if (!opt.out_dir.empty()) {
        fs::create_directories(opt.out_dir);
        return opt.out_dir / in.filename();
    }
    auto out = in;
    out.replace_filename(in.stem().string() + ".clean" + in.extension().string());
    return out;
}

// -------------------------------------------------------------
// Clean image using Exiv2
// -------------------------------------------------------------
static bool clean_image(const fs::path& in, const fs::path& out, const Options& opt) {
    try {
        fs::path target = opt.in_place ? in : out;

        if (!opt.in_place) {
            fs::copy_file(in, out, fs::copy_options::overwrite_existing);
        } else if (opt.backup) {
            fs::path bak = in;
            bak += ".bak";
            if (!fs::exists(bak)) fs::copy_file(in, bak);
        }

        auto image = Exiv2::ImageFactory::open(target.string());
        if (!image) {
            return false;
        }
        image->readMetadata();
        size_t before = image->exifData().count() + image->iptcData().size() + image->xmpData().count();

        image->exifData().clear();
        image->iptcData().clear();
        image->xmpData().clear();
        image->writeMetadata();

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

// -------------------------------------------------------------
// Clean PDF using qpdf
// -------------------------------------------------------------
static bool clean_pdf(const fs::path& in, const fs::path& out, const Options& opt) {
    std::string qpdf = find_qpdf();
    if (qpdf.empty()) {
        return false;
    }

    if (opt.in_place) {
        if (opt.backup) {
            fs::path bak = in;
            bak += ".bak";
            if (!fs::exists(bak)) fs::copy_file(in, bak);
        }
        fs::path tmp = in;
        tmp += ".tmp.pdf";
        std::string cmd = shell_escape(qpdf) +
            " --clear-metadata --empty-xmp --linearize " +
            shell_escape(in.string()) + " " + shell_escape(tmp.string());
        int rc = std::system(cmd.c_str());
        if (rc == 0) {
            fs::rename(tmp, in);
            return true;
        }
        return false;
    } else {
        std::string cmd = shell_escape(qpdf) +
            " --clear-metadata --empty-xmp --linearize " +
            shell_escape(in.string()) + " " + shell_escape(out.string());
        int rc = std::system(cmd.c_str());
        return rc == 0;
    }
}
