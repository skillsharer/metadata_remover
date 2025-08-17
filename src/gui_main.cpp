#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Scroll.H>

#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include <sstream>

#include "metadata_core.h"

// GUI Application Class
class CleanMetaGUI {
private:
    Fl_Window* main_window;
    Fl_Button* select_files_btn;
    Fl_Button* process_btn;
    Fl_Check_Button* in_place_check;
    Fl_Check_Button* backup_check;
    Fl_Check_Button* recursive_check;
    Fl_Input* output_dir_input;
    Fl_Button* browse_output_btn;
    Fl_Text_Display* file_list_display;
    Fl_Text_Buffer* file_list_buffer;
    Fl_Text_Display* log_display;
    Fl_Text_Buffer* log_buffer;
    Fl_Progress* progress_bar;
    Fl_Box* status_box;
    
    // Application state
    std::vector<std::string> selected_files;
    std::vector<std::string> log_messages;
    bool processing = false;
    std::atomic<int> progress{0};
    std::atomic<int> total_files{0};
    std::mutex log_mutex;
    std::queue<std::string> log_queue;
    
public:
    CleanMetaGUI() {
        create_gui();
        setup_callbacks();
    }
    
    ~CleanMetaGUI() {
        delete file_list_buffer;
        delete log_buffer;
        delete main_window;
    }
    
    void show() {
        main_window->show();
    }
    
    void run() {
        Fl::run();
    }
    
private:
    void create_gui() {
        // Main window
        main_window = new Fl_Window(1000, 700, "CleanMeta - Metadata Remover");
        main_window->color(FL_DARK2);
        
        // Header
        Fl_Box* title = new Fl_Box(20, 20, 960, 40, "🧹 CleanMeta - Metadata Remover");
        title->labelfont(FL_BOLD);
        title->labelsize(18);
        title->labelcolor(FL_WHITE);
        title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        Fl_Box* subtitle = new Fl_Box(20, 50, 960, 20, "Remove metadata from your files");
        subtitle->labelsize(12);
        subtitle->labelcolor(FL_LIGHT2);
        subtitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        // Left panel - File selection and options
        Fl_Group* left_panel = new Fl_Group(20, 90, 450, 560);
        left_panel->box(FL_BORDER_BOX);
        left_panel->color(FL_DARK1);
        
        // File selection
        select_files_btn = new Fl_Button(40, 110, 200, 40, "📁 Select Files");
        select_files_btn->color(FL_BLUE);
        select_files_btn->labelcolor(FL_WHITE);
        select_files_btn->labelfont(FL_BOLD);
        select_files_btn->tooltip("Click to select multiple files to clean");
        
        // File list display
        Fl_Box* files_label = new Fl_Box(40, 160, 200, 20, "Selected Files:");
        files_label->labelcolor(FL_WHITE);
        files_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        file_list_buffer = new Fl_Text_Buffer();
        file_list_display = new Fl_Text_Display(40, 180, 410, 150);
        file_list_display->buffer(file_list_buffer);
        file_list_display->color(FL_DARK3);
        file_list_display->textcolor(FL_LIGHT2);
        file_list_display->wrap_mode(1, 80);
        
        // Options section
        Fl_Box* options_label = new Fl_Box(40, 340, 200, 20, "Options:");
        options_label->labelcolor(FL_WHITE);
        options_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        options_label->labelfont(FL_BOLD);
        
        in_place_check = new Fl_Check_Button(40, 370, 200, 25, "Clean files in place");
        in_place_check->labelcolor(FL_WHITE);
        in_place_check->value(0);
        
        backup_check = new Fl_Check_Button(60, 400, 200, 25, "Create backup files (.bak)");
        backup_check->labelcolor(FL_WHITE);
        backup_check->value(1);
        
        recursive_check = new Fl_Check_Button(40, 430, 200, 25, "Process directories recursively");
        recursive_check->labelcolor(FL_WHITE);
        recursive_check->value(0);
        
        // Output directory
        Fl_Box* output_label = new Fl_Box(40, 460, 200, 20, "Output Directory:");
        output_label->labelcolor(FL_WHITE);
        output_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        output_dir_input = new Fl_Input(40, 480, 310, 30);
        output_dir_input->color(FL_DARK3);
        output_dir_input->textcolor(FL_WHITE);
        
        browse_output_btn = new Fl_Button(360, 480, 90, 30, "Browse...");
        browse_output_btn->color(FL_DARK2);
        browse_output_btn->labelcolor(FL_WHITE);
        
        // Process button
        process_btn = new Fl_Button(40, 530, 410, 50, "🚀 Clean Metadata");
        process_btn->color(FL_GREEN);
        process_btn->labelcolor(FL_WHITE);
        process_btn->labelfont(FL_BOLD);
        process_btn->labelsize(14);
        
        // Progress bar
        progress_bar = new Fl_Progress(40, 590, 410, 20);
        progress_bar->color(FL_DARK3);
        progress_bar->selection_color(FL_BLUE);
        progress_bar->hide();
        
        // Status
        status_box = new Fl_Box(40, 620, 410, 20, "Ready to process files");
        status_box->labelcolor(FL_LIGHT2);
        status_box->labelsize(10);
        status_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        left_panel->end();
        
        // Right panel - Log output
        Fl_Group* right_panel = new Fl_Group(490, 90, 490, 560);
        right_panel->box(FL_BORDER_BOX);
        right_panel->color(FL_DARK1);
        
        Fl_Box* log_label = new Fl_Box(510, 110, 200, 20, "📋 Processing Log:");
        log_label->labelcolor(FL_WHITE);
        log_label->labelfont(FL_BOLD);
        log_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        log_buffer = new Fl_Text_Buffer();
        log_display = new Fl_Text_Display(510, 140, 450, 500);
        log_display->buffer(log_buffer);
        log_display->color(FL_BLACK);
        log_display->textcolor(FL_WHITE);
        log_display->wrap_mode(1, 80);
        
        right_panel->end();
        
        main_window->end();
        main_window->resizable(main_window);
        
        // Set initial state
        update_ui_state();
    }
    
    void setup_callbacks() {
        select_files_btn->callback(select_files_cb, this);
        process_btn->callback(process_files_cb, this);
        browse_output_btn->callback(browse_output_cb, this);
        in_place_check->callback(in_place_cb, this);
        
        // Timer for updating UI
        Fl::add_timeout(0.1, update_timer_cb, this);
    }
    
    static void select_files_cb(Fl_Widget*, void* data) {
        static_cast<CleanMetaGUI*>(data)->select_files();
    }
    
    static void process_files_cb(Fl_Widget*, void* data) {
        static_cast<CleanMetaGUI*>(data)->process_files();
    }
    
    static void browse_output_cb(Fl_Widget*, void* data) {
        static_cast<CleanMetaGUI*>(data)->browse_output_directory();
    }
    
    static void in_place_cb(Fl_Widget*, void* data) {
        static_cast<CleanMetaGUI*>(data)->update_ui_state();
    }
    
    static void update_timer_cb(void* data) {
        static_cast<CleanMetaGUI*>(data)->update_ui();
        Fl::repeat_timeout(0.1, update_timer_cb, data);
    }
    
    void select_files() {
        // Use native file chooser for better multiple file selection
        Fl_Native_File_Chooser chooser;
        chooser.title("Select files to clean");
        chooser.type(Fl_Native_File_Chooser::BROWSE_MULTI_FILE);
        chooser.filter("Image and PDF Files\t*.{jpg,jpeg,png,heic,pdf}\n"
                      "Image Files\t*.{jpg,jpeg,png,heic}\n"
                      "PDF Files\t*.pdf\n"
                      "All Files\t*");
        
        switch (chooser.show()) {
            case 0:  // User picked files
                selected_files.clear();
                for (int i = 0; i < chooser.count(); i++) {
                    selected_files.push_back(std::string(chooser.filename(i)));
                }
                update_file_list();
                update_ui_state();
                break;
            case 1:  // User cancelled
                break;
            default:  // Error
                fl_alert("Error opening file chooser: %s", chooser.errmsg());
                break;
        }
    }
    
    void browse_output_directory() {
        const char* dirname = fl_dir_chooser("Select output directory", nullptr);
        if (dirname) {
            output_dir_input->value(dirname);
        }
    }
    
    void update_file_list() {
        std::stringstream ss;
        if (selected_files.empty()) {
            ss << "No files selected.\nClick 'Select Files' to choose files to process.";
        } else {
            ss << "Selected " << selected_files.size() << " file(s):\n\n";
            for (size_t i = 0; i < selected_files.size(); ++i) {
                fs::path path(selected_files[i]);
                ss << (i + 1) << ". " << path.filename().string();
                
                // Show file type
                if (is_image(path)) {
                    ss << " [IMAGE]";
                } else if (is_pdf(path)) {
                    ss << " [PDF]";
                } else {
                    ss << " [UNSUPPORTED]";
                }
                
                ss << "\n";
                
                // Show directory on next line for better readability
                ss << "   " << path.parent_path().string() << "\n\n";
            }
        }
        file_list_buffer->text(ss.str().c_str());
    }
    
    void update_ui_state() {
        bool in_place = in_place_check->value();
        
        if (in_place) {
            backup_check->show();
            output_dir_input->hide();
            browse_output_btn->hide();
        } else {
            backup_check->hide();
            output_dir_input->show();
            browse_output_btn->show();
        }
        
        bool can_process = !selected_files.empty() && !processing;
        if (can_process) {
            process_btn->activate();
        } else {
            process_btn->deactivate();
        }
        
        main_window->redraw();
    }
    
    void process_files() {
        if (processing || selected_files.empty()) return;
        
        processing = true;
        progress = 0;
        total_files = selected_files.size();
        
        // Clear log
        log_buffer->text("");
        log_messages.clear();
        
        // Show progress bar
        progress_bar->show();
        status_box->label("Processing files...");
        
        update_ui_state();
        
        // Start processing in separate thread
        std::thread(&CleanMetaGUI::process_files_worker, this).detach();
    }
    
    void process_files_worker() {
        Options opt;
        opt.in_place = in_place_check->value();
        opt.backup = backup_check->value();
        opt.recursive = recursive_check->value();
        
        if (!opt.in_place && output_dir_input->value()) {
            opt.out_dir = output_dir_input->value();
        }
        
        int processed = 0;
        int successful = 0;
        
        for (const auto& file_path : selected_files) {
            fs::path path(file_path);
            
            std::string message;
            try {
                fs::path out = opt.in_place ? path : default_output(path, opt);
                bool success = false;
                
                if (is_image(path)) {
                    success = clean_image(path, out, opt);
                    if (success) {
                        message = "[OK] " + path.filename().string() + " (image) - metadata removed";
                    } else {
                        message = "[ERROR] Failed to process image: " + path.filename().string();
                    }
                } else if (is_pdf(path)) {
                    success = clean_pdf(path, out, opt);
                    if (success) {
                        message = "[OK] " + path.filename().string() + " (PDF) - metadata removed";
                    } else {
                        message = "[ERROR] Failed to process PDF: " + path.filename().string();
                    }
                } else {
                    message = "[WARNING] Unsupported file type: " + path.filename().string();
                }
                
                if (success) successful++;
                
            } catch (const std::exception& e) {
                message = "[ERROR] " + path.filename().string() + " - " + e.what();
            }
            
            // Add message to queue
            {
                std::lock_guard<std::mutex> lock(log_mutex);
                log_queue.push(message);
            }
            
            processed++;
            progress = processed;
            
            // Small delay to make progress visible
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Final summary message
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            log_queue.push("\n=== PROCESSING COMPLETE ===");
            log_queue.push("Successfully processed " + std::to_string(successful) + 
                          " out of " + std::to_string(processed) + " files.");
        }
        
        processing = false;
    }
    
    void update_ui() {
        // Update log messages from worker thread
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            while (!log_queue.empty()) {
                log_messages.push_back(log_queue.front());
                log_queue.pop();
                
                // Update log display
                std::string current_text = log_buffer->text() ? log_buffer->text() : "";
                current_text += log_messages.back() + "\n";
                log_buffer->text(current_text.c_str());
                
                // Auto-scroll to bottom
                log_display->insert_position(log_buffer->length());
                log_display->show_insert_position();
            }
        }
        
        // Update progress bar
        if (processing && total_files > 0) {
            double percentage = (double)progress / total_files * 100.0;
            progress_bar->value(percentage);
            
            std::string status = "Processing: " + std::to_string(progress) + 
                               " / " + std::to_string(total_files);
            status_box->label(status.c_str());
        } else if (!processing && progress > 0) {
            progress_bar->hide();
            status_box->label("Processing complete!");
        }
        
        update_ui_state();
    }
};

int main() {
    // Set FLTK scheme for modern look
    Fl::scheme("gtk+");
    
    // Create and show the GUI
    CleanMetaGUI app;
    app.show();
    
    return Fl::run();
}
