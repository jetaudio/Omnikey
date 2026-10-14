//
//  main.cpp
//  Omnikey - Linux setup app (GTK 4 + libadwaita)
//
//  Edits ~/.config/omnikey/omnikey.conf and macros.txt; the IBus engine watches
//  both files and applies changes straight away.
//

#include "Config.h"
#include "Engine.h"

#include <adwaita.h>
#include <string>
#include <vector>

#ifndef OMNIKEY_VERSION
#define OMNIKEY_VERSION "dev"
#endif

#define APP_ID "io.github.jetaudio.Omnikey"

static GtkWindow* mainWindow = NULL;
static AdwPreferencesGroup* macroGroup = NULL;
static std::vector<GtkWidget*> macroRows;
static AdwBanner* sourceBanner = NULL;

/*------------------------------------------------------------------------------*/
// Settings helpers

// Reloads first so a language switch made from the keyboard is not overwritten.
static void storeSetting(int* setting, int value) {
    OmnikeyConfig::load();
    *setting = value;
    OmnikeyConfig::save();
}

static void onSwitchChanged(AdwSwitchRow* row, GParamSpec*, gpointer data) {
    storeSetting((int*)data, adw_switch_row_get_active(row) ? 1 : 0);
}

static GtkWidget* switchRow(const char* title, const char* subtitle, int* setting) {
    GtkWidget* row = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    if (subtitle) adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
    adw_switch_row_set_active(ADW_SWITCH_ROW(row), *setting != 0);
    g_signal_connect(row, "notify::active", G_CALLBACK(onSwitchChanged), setting);
    return row;
}

static void onComboChanged(AdwComboRow* row, GParamSpec*, gpointer data) {
    storeSetting((int*)data, (int)adw_combo_row_get_selected(row));
}

static GtkWidget* comboRow(const char* title, const char* subtitle, const char* const* items, int* setting) {
    GtkWidget* row = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    if (subtitle) adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
    GtkStringList* model = gtk_string_list_new(items);
    adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(model));
    g_object_unref(model);
    adw_combo_row_set_selected(ADW_COMBO_ROW(row), (guint)*setting);
    g_signal_connect(row, "notify::selected", G_CALLBACK(onComboChanged), setting);
    return row;
}

// The input mode is stored as vLanguage plus vChineseMode: V, E or 中.
static void onModeChanged(AdwComboRow* row, GParamSpec*, gpointer) {
    guint mode = adw_combo_row_get_selected(row);
    OmnikeyConfig::load();
    vLanguage = mode == 0 ? 1 : 0;
    vChineseMode = mode == 2 ? 1 : 0;
    OmnikeyConfig::save();
}

static GtkWidget* modeRow() {
    static const char* const modes[] = {"Tiếng Việt", "English", "中文 (Pinyin)", NULL};
    GtkWidget* row = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), "Chế độ gõ");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Tiếng Trung dùng Pinyin giản thể qua Rime");
    GtkStringList* model = gtk_string_list_new(modes);
    adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(model));
    g_object_unref(model);
    adw_combo_row_set_selected(ADW_COMBO_ROW(row), vChineseMode ? 2 : (vLanguage ? 0 : 1));
    g_signal_connect(row, "notify::selected", G_CALLBACK(onModeChanged), NULL);
    return row;
}

static GtkWidget* group(const char* title, const char* description) {
    GtkWidget* g = adw_preferences_group_new();
    if (title) adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(g), title);
    if (description) adw_preferences_group_set_description(ADW_PREFERENCES_GROUP(g), description);
    return g;
}

static void addRow(GtkWidget* g, GtkWidget* row) {
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(g), row);
}

/*------------------------------------------------------------------------------*/
// Input source banner: offers to add Omnikey to GNOME's input sources.

static GSettings* inputSourceSettings() {
    GSettingsSchemaSource* source = g_settings_schema_source_get_default();
    if (!source) return NULL;
    GSettingsSchema* schema = g_settings_schema_source_lookup(source, "org.gnome.desktop.input-sources", TRUE);
    if (!schema) return NULL;
    g_settings_schema_unref(schema);
    return g_settings_new("org.gnome.desktop.input-sources");
}

static bool hasOmnikeySource(GSettings* settings) {
    GVariant* sources = g_settings_get_value(settings, "sources");
    bool found = false;
    GVariantIter iter;
    const char *type, *id;
    g_variant_iter_init(&iter, sources);
    while (g_variant_iter_next(&iter, "(&s&s)", &type, &id)) {
        if (g_strcmp0(type, "ibus") == 0 && g_strcmp0(id, "omnikey") == 0) found = true;
    }
    g_variant_unref(sources);
    return found;
}

static void onAddSource(AdwBanner* banner, gpointer) {
    GSettings* settings = inputSourceSettings();
    if (!settings) return;
    GVariant* sources = g_settings_get_value(settings, "sources");
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    GVariantIter iter;
    const char *type, *id;
    g_variant_iter_init(&iter, sources);
    while (g_variant_iter_next(&iter, "(&s&s)", &type, &id)) g_variant_builder_add(&builder, "(ss)", type, id);
    g_variant_builder_add(&builder, "(ss)", "ibus", "omnikey");
    g_settings_set_value(settings, "sources", g_variant_builder_end(&builder));
    g_variant_unref(sources);
    g_object_unref(settings);
    adw_banner_set_revealed(banner, FALSE);
}

// The IBus engine was called "openkey" before the rename; point an existing
// input source at the new engine so it keeps its place in the list.
static void migrateOpenKeySource(GSettings* settings) {
    GVariant* sources = g_settings_get_value(settings, "sources");
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    bool changed = false, hasNew = false;
    GVariantIter iter;
    const char *type, *id;
    g_variant_iter_init(&iter, sources);
    while (g_variant_iter_next(&iter, "(&s&s)", &type, &id))
        if (g_strcmp0(type, "ibus") == 0 && g_strcmp0(id, "omnikey") == 0) hasNew = true;
    g_variant_iter_init(&iter, sources);
    while (g_variant_iter_next(&iter, "(&s&s)", &type, &id)) {
        if (g_strcmp0(type, "ibus") == 0 && g_strcmp0(id, "openkey") == 0) {
            changed = true;
            if (!hasNew) g_variant_builder_add(&builder, "(ss)", "ibus", "omnikey");
            hasNew = true;
        } else {
            g_variant_builder_add(&builder, "(ss)", type, id);
        }
    }
    if (changed) g_settings_set_value(settings, "sources", g_variant_builder_end(&builder));
    else g_variant_builder_clear(&builder);
    g_variant_unref(sources);
}

static GtkWidget* buildBanner() {
    GtkWidget* banner = adw_banner_new("Omnikey chưa có trong danh sách nguồn nhập của hệ thống");
    adw_banner_set_button_label(ADW_BANNER(banner), "Thêm Omnikey");
    g_signal_connect(banner, "button-clicked", G_CALLBACK(onAddSource), NULL);
    GSettings* settings = inputSourceSettings();
    if (settings) migrateOpenKeySource(settings);
    adw_banner_set_revealed(ADW_BANNER(banner), settings && !hasOmnikeySource(settings));
    if (settings) g_object_unref(settings);
    sourceBanner = ADW_BANNER(banner);
    return banner;
}

/*------------------------------------------------------------------------------*/
// Pages

static void onOutputModeToggled(GtkCheckButton* button, gpointer data) {
    if (gtk_check_button_get_active(button)) storeSetting(&vOutputMode, GPOINTER_TO_INT(data));
}

static GtkWidget* outputModeRow(const char* title, const char* subtitle, int mode, GtkCheckButton* groupWith) {
    GtkWidget* row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
    GtkWidget* check = gtk_check_button_new();
    gtk_widget_set_valign(check, GTK_ALIGN_CENTER);
    if (groupWith) gtk_check_button_set_group(GTK_CHECK_BUTTON(check), groupWith);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), vOutputMode == mode);
    g_signal_connect(check, "toggled", G_CALLBACK(onOutputModeToggled), GINT_TO_POINTER(mode));
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), check);
    adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), check);
    return row;
}

static GtkWidget* typingPage() {
    GtkWidget* page = adw_preferences_page_new();

    GtkWidget* input = group("Bộ gõ", NULL);
    addRow(input, modeRow());
    static const char* const inputTypes[] = {"Telex", "VNI", "Simple Telex 1", "Simple Telex 2", NULL};
    addRow(input, comboRow("Kiểu gõ", NULL, inputTypes, &vInputType));
    static const char* const codeTables[] = {"Unicode dựng sẵn", "TCVN3 (ABC)", "VNI Windows",
                                             "Unicode tổ hợp", "Vietnamese Locale CP 1258", NULL};
    addRow(input, comboRow("Bảng mã", NULL, codeTables, &vCodeTable));
    static const char* const switchKeys[] = {"Không dùng", "Ctrl + Shift", "Ctrl + Space", "Alt + Z", NULL};
    addRow(input, comboRow("Phím chuyển chế độ", "Xoay vòng Tiếng Việt → English → 中; cũng chọn được trên thanh trạng thái", switchKeys, &vSwitchKey));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(input));

    GtkWidget* output = group("Cách đưa chữ vào ứng dụng", NULL);
    GtkWidget* preedit = outputModeRow("Gạch chân từ đang gõ",
        "Ổn định trong mọi ứng dụng, kể cả terminal", OutputPreedit, NULL);
    GtkCheckButton* first = GTK_CHECK_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(preedit)));
    addRow(output, preedit);
    addRow(output, outputModeRow("Sửa trực tiếp",
        "Không gạch chân; một số ứng dụng có thể gõ sai", OutputDirect, first));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(output));

    GtkWidget* quick = group("Gõ nhanh", NULL);
    addRow(quick, switchRow("Gõ nhanh Telex", "cc → ch, gg → gi, kk → kh, nn → ng, qq → qu, pp → ph, tt → th", &vQuickTelex));
    addRow(quick, switchRow("Phụ âm đầu nhanh", "f → ph, j → gi, w → qu", &vQuickStartConsonant));
    addRow(quick, switchRow("Phụ âm cuối nhanh", "g → ng, h → nh, k → ch", &vQuickEndConsonant));
    addRow(quick, switchRow("Viết hoa chữ cái đầu câu", NULL, &vUpperCaseFirstChar));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(quick));
    return page;
}

static GtkWidget* spellingPage() {
    GtkWidget* page = adw_preferences_page_new();

    GtkWidget* spelling = group("Chính tả", NULL);
    addRow(spelling, switchRow("Kiểm tra chính tả", "Không bỏ dấu khi từ không phải tiếng Việt", &vCheckSpelling));
    addRow(spelling, switchRow("Tự khôi phục phím với từ sai", "Ví dụ gõ “text” không thành “tẽt”", &vRestoreIfWrongSpelling));
    addRow(spelling, switchRow("Cho phép phụ âm Z, F, W, J", NULL, &vAllowConsonantZFWJ));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(spelling));

    GtkWidget* marks = group("Bỏ dấu", NULL);
    addRow(marks, switchRow("Đặt dấu kiểu mới", "oà, uý thay cho òa, úy", &vUseModernOrthography));
    addRow(marks, switchRow("Bỏ dấu tự do", "Đặt dấu ở bất kỳ đâu trong từ", &vFreeMark));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(marks));

    GtkWidget* temp = group("Tạm tắt", NULL);
    addRow(temp, switchRow("Tạm tắt chính tả bằng phím Ctrl", "Bấm rồi thả Ctrl để bật / tắt cho từ đang gõ", &vTempOffSpelling));
    addRow(temp, switchRow("Tạm tắt Omnikey bằng phím Alt", "Bấm rồi thả Alt để gõ tiếng Anh cho từ đang gõ", &vTempOffOmnikey));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(temp));
    return page;
}

/*------------------------------------------------------------------------------*/
// Macros

static void rebuildMacroRows();

static void onDeleteMacro(GtkButton*, gpointer data) {
    deleteMacro((const char*)data);
    OmnikeyConfig::saveMacros();
    rebuildMacroRows();
}

static void rebuildMacroRows() {
    for (GtkWidget* row : macroRows) adw_preferences_group_remove(macroGroup, row);
    macroRows.clear();

    vector<vector<Uint32>> keys;
    vector<string> texts, contents;
    getAllMacro(keys, texts, contents);
    for (size_t i = 0; i < texts.size(); i++) {
        GtkWidget* row = adw_action_row_new();
        gchar* title = g_markup_escape_text(texts[i].c_str(), -1);
        gchar* subtitle = g_markup_escape_text(contents[i].c_str(), -1);
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
        g_free(title);
        g_free(subtitle);

        GtkWidget* remove = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_widget_set_valign(remove, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(remove, "flat");
        gtk_widget_set_tooltip_text(remove, "Xoá");
        g_signal_connect_data(remove, "clicked", G_CALLBACK(onDeleteMacro),
                              g_strdup(texts[i].c_str()), (GClosureNotify)(void (*)(void))g_free, (GConnectFlags)0);
        adw_action_row_add_suffix(ADW_ACTION_ROW(row), remove);

        adw_preferences_group_add(macroGroup, row);
        macroRows.push_back(row);
    }
    if (texts.empty()) {
        GtkWidget* row = adw_action_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), "Chưa có từ gõ tắt nào");
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Bấm + để thêm, ví dụ “vn” → “Việt Nam”");
        gtk_widget_add_css_class(row, "dim-label");
        adw_preferences_group_add(macroGroup, row);
        macroRows.push_back(row);
    }
}

struct MacroDialogData {
    GtkWidget* name;
    GtkWidget* content;
};

static void onMacroDialogResponse(AdwAlertDialog*, const char* response, gpointer data) {
    MacroDialogData* fields = (MacroDialogData*)data;
    if (g_strcmp0(response, "add") == 0) {
        std::string name = gtk_editable_get_text(GTK_EDITABLE(fields->name));
        std::string content = gtk_editable_get_text(GTK_EDITABLE(fields->content));
        if (!name.empty() && !content.empty() && name.find(':') == std::string::npos) {
            addMacro(name, content);
            OmnikeyConfig::saveMacros();
            rebuildMacroRows();
        }
    }
    delete fields;
}

static void onMacroFieldsChanged(GtkEditable*, gpointer data) {
    AdwAlertDialog* dialog = ADW_ALERT_DIALOG(data);
    MacroDialogData* fields = (MacroDialogData*)g_object_get_data(G_OBJECT(dialog), "fields");
    const char* name = gtk_editable_get_text(GTK_EDITABLE(fields->name));
    const char* content = gtk_editable_get_text(GTK_EDITABLE(fields->content));
    adw_alert_dialog_set_response_enabled(dialog, "add", *name && *content && !strchr(name, ':'));
}

static void onAddMacro(GtkButton*, gpointer) {
    AdwDialog* dialog = adw_alert_dialog_new("Thêm từ gõ tắt", "Gõ từ tắt rồi bấm phím cách để thay bằng nội dung đầy đủ.");
    AdwAlertDialog* alert = ADW_ALERT_DIALOG(dialog);
    adw_alert_dialog_add_responses(alert, "cancel", "Huỷ", "add", "Thêm", NULL);
    adw_alert_dialog_set_response_appearance(alert, "add", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response(alert, "add");
    adw_alert_dialog_set_close_response(alert, "cancel");
    adw_alert_dialog_set_response_enabled(alert, "add", FALSE);

    GtkWidget* list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(list, "boxed-list");
    MacroDialogData* fields = new MacroDialogData();
    fields->name = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fields->name), "Từ gõ tắt");
    fields->content = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fields->content), "Nội dung");
    adw_entry_row_set_activates_default(ADW_ENTRY_ROW(fields->content), TRUE);
    gtk_list_box_append(GTK_LIST_BOX(list), fields->name);
    gtk_list_box_append(GTK_LIST_BOX(list), fields->content);
    adw_alert_dialog_set_extra_child(alert, list);

    g_object_set_data(G_OBJECT(dialog), "fields", fields);
    g_signal_connect(fields->name, "changed", G_CALLBACK(onMacroFieldsChanged), dialog);
    g_signal_connect(fields->content, "changed", G_CALLBACK(onMacroFieldsChanged), dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(onMacroDialogResponse), fields);
    adw_dialog_present(dialog, GTK_WIDGET(mainWindow));
    gtk_widget_grab_focus(fields->name);
}

static GtkWidget* macroPage() {
    GtkWidget* page = adw_preferences_page_new();

    GtkWidget* options = group(NULL, NULL);
    addRow(options, switchRow("Dùng gõ tắt", NULL, &vUseMacro));
    addRow(options, switchRow("Tự viết hoa theo từ gõ tắt", "btw → by the way, Btw → By the way, BTW → BY THE WAY", &vAutoCapsMacro));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(options));

    GtkWidget* list = group("Danh sách gõ tắt", NULL);
    GtkWidget* add = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(add, "flat");
    gtk_widget_set_tooltip_text(add, "Thêm từ gõ tắt");
    g_signal_connect(add, "clicked", G_CALLBACK(onAddMacro), NULL);
    adw_preferences_group_set_header_suffix(ADW_PREFERENCES_GROUP(list), add);
    macroGroup = ADW_PREFERENCES_GROUP(list);
    rebuildMacroRows();
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(list));
    return page;
}

/*------------------------------------------------------------------------------*/
// Window

static void onAbout(GSimpleAction*, GVariant*, gpointer) {
    AdwDialog* about = adw_about_dialog_new();
    AdwAboutDialog* dialog = ADW_ABOUT_DIALOG(about);
    adw_about_dialog_set_application_name(dialog, "Omnikey");
    adw_about_dialog_set_application_icon(dialog, APP_ID);
    adw_about_dialog_set_version(dialog, OMNIKEY_VERSION);
    adw_about_dialog_set_developer_name(dialog, "Mai Vũ Tuyên");
    adw_about_dialog_set_comments(dialog, "Bộ gõ tiếng Việt mã nguồn mở, chạy trên IBus.");
    adw_about_dialog_set_website(dialog, "https://github.com/jetaudio/Omnikey");
    adw_about_dialog_set_issue_url(dialog, "https://github.com/jetaudio/Omnikey/issues");
    adw_about_dialog_set_license_type(dialog, GTK_LICENSE_GPL_3_0);
    adw_about_dialog_set_copyright(dialog, "© 2019 Mai Vũ Tuyên");
    adw_dialog_present(about, GTK_WIDGET(mainWindow));
}

static void addPage(AdwViewStack* stack, GtkWidget* page, const char* name, const char* title, const char* icon) {
    adw_view_stack_add_titled_with_icon(stack, page, name, title, icon);
}

static void onActivate(GtkApplication* app, gpointer) {
    if (mainWindow) {
        gtk_window_present(mainWindow);
        return;
    }
    OmnikeyConfig::load();
    OmnikeyConfig::loadMacros();

    GtkWidget* window = adw_application_window_new(app);
    mainWindow = GTK_WINDOW(window);
    gtk_window_set_title(mainWindow, "Omnikey");
    gtk_window_set_icon_name(mainWindow, APP_ID);
    gtk_window_set_default_size(mainWindow, 640, 720);
    gtk_widget_set_size_request(window, 360, 400);

    GtkWidget* stack = adw_view_stack_new();
    addPage(ADW_VIEW_STACK(stack), typingPage(), "typing", "Gõ phím", "input-keyboard-symbolic");
    addPage(ADW_VIEW_STACK(stack), spellingPage(), "spelling", "Chính tả", "tools-check-spelling-symbolic");
    addPage(ADW_VIEW_STACK(stack), macroPage(), "macro", "Gõ tắt", "accessories-text-editor-symbolic");

    GtkWidget* header = adw_header_bar_new();
    GtkWidget* switcher = adw_view_switcher_new();
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_STACK(stack));
    adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), switcher);

    GMenu* menu = g_menu_new();
    g_menu_append(menu, "Giới thiệu Omnikey", "app.about");
    GtkWidget* menuButton = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menuButton), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menuButton), G_MENU_MODEL(menu));
    gtk_menu_button_set_primary(GTK_MENU_BUTTON(menuButton), TRUE);
    gtk_widget_set_tooltip_text(menuButton, "Menu chính");
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), menuButton);
    g_object_unref(menu);

    GtkWidget* switcherBar = adw_view_switcher_bar_new();
    adw_view_switcher_bar_set_stack(ADW_VIEW_SWITCHER_BAR(switcherBar), ADW_VIEW_STACK(stack));

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(content), buildBanner());
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_box_append(GTK_BOX(content), stack);

    GtkWidget* toolbar = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);
    adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(toolbar), switcherBar);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), toolbar);

    // Narrow windows move the page switcher to the bottom, like GNOME apps do.
    AdwBreakpoint* breakpoint = adw_breakpoint_new(adw_breakpoint_condition_parse("max-width: 500sp"));
    GValue on = G_VALUE_INIT, none = G_VALUE_INIT;
    g_value_init(&on, G_TYPE_BOOLEAN);
    g_value_set_boolean(&on, TRUE);
    g_value_init(&none, G_TYPE_OBJECT);
    g_value_set_object(&none, NULL);
    adw_breakpoint_add_setter(breakpoint, G_OBJECT(switcherBar), "reveal", &on);
    adw_breakpoint_add_setter(breakpoint, G_OBJECT(header), "title-widget", &none);
    adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), breakpoint);
    g_value_unset(&on);
    g_value_unset(&none);

    gtk_window_present(mainWindow);
}

int main(int argc, char** argv) {
    AdwApplication* app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);

    GSimpleAction* about = g_simple_action_new("about", NULL);
    g_signal_connect(about, "activate", G_CALLBACK(onAbout), NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(about));
    g_object_unref(about);
    const char* closeAccels[] = {"<Control>w", "<Control>q", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "window.close", closeAccels);

    g_signal_connect(app, "activate", G_CALLBACK(onActivate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
