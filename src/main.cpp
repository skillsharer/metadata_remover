#include <exiv2/exiv2.hpp>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

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
// Config options
// -------------------------------------------------------------
struct Options {
    bool in_place = false;
    bool backup = true;
    bool recursive = false;
    fs::path out_dir;
};

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
            std::cerr << "[ERR] cannot open " << in << "\n";
            return false;
        }
        image->readMetadata();
        size_t before = image->exifData().count() + image->iptcData().size() + image->xmpData().count();

        image->exifData().clear();
        image->iptcData().clear();
        image->xmpData().clear();
        image->writeMetadata();

        image = Exiv2::ImageFactory::open(target.string());
        image->readMetadata();
        size_t after = image->exifData().count() + image->iptcData().size() + image->xmpData().count();

        std::cout << "[OK] " << in.filename().string()
                  << " (img) removed " << before << " tags, remaining " << after << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERR] " << in << " : " << e.what() << "\n";
        return false;
    }
}

// -------------------------------------------------------------
// Clean PDF using qpdf
// -------------------------------------------------------------
static bool clean_pdf(const fs::path& in, const fs::path& out, const Options& opt) {
    std::string qpdf = find_qpdf();
    if (qpdf.empty()) {
        std::cerr << "[ERR] qpdf not found (bundle it under bin/)\n";
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
            std::cout << "[OK] " << in.filename().string() << " (pdf) metadata cleared\n";
            return true;
        }
        return false;
    } else {
        std::string cmd = shell_escape(qpdf) +
            " --clear-metadata --empty-xmp --linearize " +
            shell_escape(in.string()) + " " + shell_escape(out.string());
        int rc = std::system(cmd.c_str());
        if (rc == 0) {
            std::cout << "[OK] " << in.filename().string() << " (pdf) metadata cleared\n";
            return true;
        }
        return false;
    }
}

// -------------------------------------------------------------
// Help
// -------------------------------------------------------------
static void usage(const char* prog) {
    std::cout <<
"cleanmeta — strip metadata from images (JPEG/PNG/HEIC) and PDFs\n\n"
"Usage:\n"
"  " << prog << " [options] <files or folders...>\n\n"
"Options:\n"
"  -o DIR, --out DIR     Write cleaned copies to DIR\n"
"  --in-place            Clean files in place (default: copy)\n"
"  --no-backup           Skip .bak backup when in-place\n"
"  -r, --recursive       Recurse into folders\n"
"  -h, --help            Show help\n";
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------
int main(int argc, char** argv) {
    Options opt;
    std::vector<fs::path> inputs;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else if (a == "-o" || a == "--out") { opt.out_dir = argv[++i]; }
        else if (a == "--in-place") opt.in_place = true;
        else if (a == "--no-backup") opt.backup = false;
        else if (a == "-r" || a == "--recursive") opt.recursive = true;
        else inputs.push_back(a);
    }
    if (inputs.empty()) { usage(argv[0]); return 1; }

    size_t total = 0, ok = 0;
    std::function<void(const fs::path&)> handle = [&](const fs::path& p) {
        if (fs::is_directory(p)) {
            if (!opt.recursive) { std::cerr << "[WARN] skipping dir " << p << "\n"; return; }
            for (auto& e : fs::recursive_directory_iterator(p)) {
                if (fs::is_regular_file(e)) handle(e.path());
            }
            return;
        }
        if (!fs::is_regular_file(p)) return;

        total++;
        fs::path out = opt.in_place ? p : default_output(p, opt);
        if (is_image(p)) {
            if (clean_image(p, out, opt)) ok++;
        } else if (is_pdf(p)) {
            if (clean_pdf(p, out, opt)) ok++;
        } else {
            std::cerr << "[WARN] unsupported: " << p << "\n";
        }
    };

    for (auto& f : inputs) handle(f);

    std::cout << "\nDone. Cleaned " << ok << " / " << total << " files.\n";
    return (ok == total) ? 0 : 2;
}
