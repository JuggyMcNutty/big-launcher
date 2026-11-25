#include <algorithm>
#include <filesystem>

#include "keyboard.hpp"
#include "../core/app.hpp"
#include "../ui/engine/focus.hpp"

namespace fs = std::filesystem;

namespace Foyer {
    using namespace ui;

    KeyboardSheet::KeyboardSheet(App &app, std::string t, std::string init, bool mask,
                                 std::function<void(const std::string&)> f)
        : app_(app), title_(std::move(t)), text_(std::move(init)), mask_(mask), on_submit_(std::move(f))
    {
        opaque = false;
        dim_below = true;
        window_ = SDL_GetRenderWindow(app_.ui().sdl());
        if (window_) SDL_StartTextInput(window_);
    }

    KeyboardSheet::~KeyboardSheet()
    {
        if (window_) SDL_StopTextInput(window_);
    }

    void KeyboardSheet::refresh()
    {
        std::string shown = text_;
        if (mask_ && !reveal_) {
            shown.clear();
            for (size_t i = 0; i < text_.size(); i++)
                if (((unsigned char) text_[i] & 0xC0) != 0x80) shown += "\xE2\x80\xA2";
        }
        field_->set(shown + "|");   // caret
    }

    static std::vector<std::vector<std::string>> layout_for(bool shift, bool symbols)
    {
        if (symbols)
            return {{"1","2","3","4","5","6","7","8","9","0"},
                    {"!","@","#","$","%","^","&","*","(",")"},
                    {"-","_","=","+","[","]","{","}",";",":"},
                    {"'","\"",",",".","/","?","\\","|","`","~"}};
        std::vector<std::vector<std::string>> rows = {
            {"1","2","3","4","5","6","7","8","9","0"},
            {"q","w","e","r","t","y","u","i","o","p"},
            {"a","s","d","f","g","h","j","k","l","-"},
            {"z","x","c","v","b","n","m",".","@","_"}};
        if (shift)
            for (auto &r : rows) for (auto &k : r) if (k.size() == 1 && isalpha((unsigned char) k[0])) k[0] = (char) toupper(k[0]);
        return rows;
    }

    void KeyboardSheet::rebuild_keys(Engine &e)
    {
        std::string focus_label;
        if (Node *f = Focus::current(); f && keys_->contains(f)) focus_label = f->id;
        keys_->clear_children();
        const Tokens &t = e.tokens();
        float avail = std::min(1500.f, e.metrics().safe_width_dp() - 80);
        float gap = 10;
        float kw = (avail - 9 * gap) / 10;
        float kh = std::min(78.f, kw * 0.78f);
        Node *refocus = nullptr;
        auto key = [&](Node *row, const std::string &label, const std::string &code, float units, bool accent = false) {
            auto *b = row->add<Button>(label);
            b->style.w = Dim::dp(kw * units + gap * (units - 1));
            b->style.h = Dim::dp(kh);
            b->style.min_h = 0;
            b->style.pad = Edges::all(4);
            b->style.justify = Justify::Center;
            b->has_bg_base_ = true;
            b->base_hi = true;
            b->radius = t.radius_sm;
            b->label->align = TextAlign::Center;
            b->label->role = TextRole::Lg;
            if (accent) b->bg = t.accent;
            b->id = "key:" + code;
            b->on_activate = [this, &e, code] { press(e, code); };
            if (b->id == focus_label) refocus = b;
            return b;
        };
        for (auto &r : layout_for(shift_, symbols_)) {
            auto *row = keys_->add<Node>();
            row->style.dir = Dir::Row; row->style.gap = gap; row->style.justify = Justify::Center;
            for (auto &k : r) key(row, k, k, 1);
        }
        auto *row = keys_->add<Node>();
        row->style.dir = Dir::Row; row->style.gap = gap; row->style.justify = Justify::Center;
        key(row, shift_ ? "\xE2\x87\xA7 ON" : "\xE2\x87\xA7", "{shift}", 1.5f);
        key(row, symbols_ ? "abc" : "#+=", "{sym}", 1.5f);
        key(row, "space", " ", 3);
        key(row, "\xE2\x8C\xAB", "{bs}", 1.5f);
        if (mask_) key(row, reveal_ ? "hide" : "show", "{reveal}", 1);
        key(row, "Done", "{ok}", mask_ ? 1.5f : 2.5f, true);
        if (refocus) Focus::set(refocus, false);
        else if (!Focus::current() || !root->contains(Focus::current())) {
            if (Node *f = Focus::first_in(keys_)) Focus::set(f, false);
        }
        invalidate_layout();
    }

    void KeyboardSheet::build(Engine &e)
    {
        const Tokens &t = e.tokens();
        root->style.dir = Dir::Column;
        root->style.justify = Justify::End;
        auto *sheet = root->add<Box>(Dir::Column);
        sheet->has_bg = true;
        sheet->bg = t.surface;
        sheet->radius = t.radius_lg;
        sheet->shadow_blur = 50;
        sheet->shadow_color = t.shadow;
        sheet->style.pad = Edges::all(30);
        sheet->style.gap = 16;
        sheet->style.align = Align::Center;
        sheet->ty.snap(120.f);
        sheet->ty.to(0.f);
        auto *title = sheet->add<Text>(title_, TextRole::Lg);
        title->style.has_self_align = true;
        title->style.self_align = Align::Start;
        auto *fbox = sheet->add<Box>(Dir::Row);
        fbox->has_bg = true;
        fbox->bg = t.bg;
        fbox->radius = t.radius_md;
        fbox->style.pad = Edges::xy(24, 16);
        fbox->style.w = Dim::fill();
        field_ = fbox->add<Text>("", TextRole::Lg);
        field_->style.grow = 1.f;
        keys_ = sheet->add<Node>();
        keys_->style.dir = Dir::Column;
        keys_->style.gap = 10;
        kit::hints(sheet, {{"A", "Type"}, {"B", "Cancel"}, {"Keys", "A physical keyboard works too"}});
        rebuild_keys(e);
        refresh();
    }

    void KeyboardSheet::press(Engine &e, const std::string &k)
    {
        if (k == "{shift}") { shift_ = !shift_; rebuild_keys(e); return; }
        if (k == "{sym}") { symbols_ = !symbols_; rebuild_keys(e); return; }
        if (k == "{reveal}") { reveal_ = !reveal_; rebuild_keys(e); refresh(); return; }
        if (k == "{bs}") {
            if (!text_.empty()) {
                size_t n = text_.size() - 1;
                while (n > 0 && ((unsigned char) text_[n] & 0xC0) == 0x80) n--;
                text_.erase(n);
            }
            refresh();
            return;
        }
        if (k == "{ok}") {
            auto cb = on_submit_;
            std::string v = text_;
            e.pop();
            if (cb) cb(v);
            return;
        }
        text_ += k;
        if (shift_ && !symbols_) { shift_ = false; rebuild_keys(e); }
        refresh();
    }

    bool KeyboardSheet::on_text(Engine &e, const std::string &t)
    {
        if (t == "\b") { press(e, "{bs}"); return true; }
        text_ += t;
        refresh();
        return true;
    }

    bool KeyboardSheet::on_action(Engine &e, Action a)
    {
        (void) e;
        (void) a;
        return false;   // Back pops (engine default)
    }

    // ---- FilePicker ----------------------------------------------------------------
    FilePicker::FilePicker(App &app, std::string title, std::string dir, std::vector<std::string> exts,
                           std::function<void(const std::string&)> f)
        : Page(app, std::move(title), dir), dir_(std::move(dir)), exts_(std::move(exts)), on_pick_(std::move(f)) {}

    void FilePicker::fill(Engine &e, Node &b)
    {
        (void) e;
        std::error_code ec;
        std::vector<fs::path> dirs = {dir_};
        for (auto &d : Paths::data_dirs()) {
            dirs.push_back(fs::path(d) / "art");
            dirs.push_back(fs::path(d) / "icons");
        }
        int n = 0;
        for (auto &d : dirs) {
            if (!fs::is_directory(d, ec)) continue;
            std::vector<fs::path> files;
            for (auto &de : fs::directory_iterator(d, ec)) {
                std::string ext = de.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (std::find(exts_.begin(), exts_.end(), ext) != exts_.end()) files.push_back(de.path());
            }
            if (files.empty()) continue;
            std::sort(files.begin(), files.end());
            kit::section(&b, d.string());
            for (auto &f : files) {
                auto *r = b.add<kit::RowButton>(app_, f.filename().string(), "");
                auto img = std::make_unique<Image>(f.string(), Fit::Contain);
                img->style.w = Dim::dp(96); img->style.h = Dim::dp(54); img->style.shrink = 0;
                r->insert(0, std::move(img));
                std::string p = f.string();
                r->on_activate = [this, p] {
                    auto cb = on_pick_;
                    app_.ui().pop();
                    if (cb) cb(p);
                };
                n++;
            }
        }
        if (!n) kit::info_row(&b, "No images found. Copy some into " + dir_, "");
    }
}
