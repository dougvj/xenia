/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <string>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/base/platform_linux.h"
#include "xenia/ui/window_gtk.h"

namespace xe {
namespace ui {

class FnWrapper {
 public:
  explicit FnWrapper(std::function<void()> fn) : fn_(std::move(fn)) {}
  void Call() { fn_(); }

 private:
  std::function<void()> fn_;
};

std::unique_ptr<Window> Window::Create(Loop* loop, const std::wstring& title) {
  return std::make_unique<GTKWindow>(loop, title);
}

GTKWindow::GTKWindow(Loop* loop, const std::wstring& title)
    : Window(loop, title) {}

GTKWindow::~GTKWindow() {
  OnDestroy();
  if (window_) {
    gtk_widget_destroy(window_);
    window_ = nullptr;
  }
}

bool GTKWindow::Initialize() { return OnCreate(); }

void gtk_event_handler_(GtkWidget* widget, GdkEvent* event, gpointer data) {
  GTKWindow* window = reinterpret_cast<GTKWindow*>(data);
  switch (event->type) {
    case GDK_OWNER_CHANGE:
      window->HandleWindowOwnerChange(&(event->owner_change));
      break;
    case GDK_VISIBILITY_NOTIFY:
      window->HandleWindowVisibility(&(event->visibility));
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      window->HandleKeyboard(&(event->key));
      break;
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_MOTION_NOTIFY:
      window->HandleMouse(&(event->any));
      break;
    case GDK_FOCUS_CHANGE:
      window->HandleWindowFocus(&(event->focus_change));
      break;
    case GDK_CONFIGURE:
      window->HandleWindowResize(&(event->configure));
    default:
      // Do nothing
      return;
  }
}

void GTKWindow::Create() {
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window_), (gchar*)title_.c_str());
  gtk_window_set_default_size(GTK_WINDOW(window_), 1280, 720);
  gtk_widget_show_all(window_);
  g_signal_connect(G_OBJECT(window_), "destroy", G_CALLBACK(gtk_main_quit),
                   NULL);
  g_signal_connect(G_OBJECT(window_), "event", G_CALLBACK(gtk_event_handler_),
                   reinterpret_cast<gpointer>(this));
}

bool GTKWindow::OnCreate() {
  loop()->PostSynchronous([this]() { this->Create(); });
  return super::OnCreate();
}

void GTKWindow::OnDestroy() { super::OnDestroy(); }

void GTKWindow::OnClose() {
  if (!closing_ && window_) {
    closing_ = true;
    gtk_widget_destroy(window_);
    window_ = nullptr;
  }
  super::OnClose();
}

bool GTKWindow::set_title(const std::wstring& title) {
  if (!super::set_title(title)) {
    return false;
  }
  gtk_window_set_title(GTK_WINDOW(window_), (gchar*)title.c_str());
  return true;
}

bool GTKWindow::SetIcon(const void* buffer, size_t size) {
  // TODO(dougvj) Set icon after changin buffer to the correct format. (the
  // call is gtk_window_set_icon)
  return false;
}

bool GTKWindow::is_fullscreen() const { return fullscreen_; }

void GTKWindow::ToggleFullscreen(bool fullscreen) {
  if (fullscreen == is_fullscreen()) {
    return;
  }

  fullscreen_ = fullscreen;

  if (fullscreen) {
    gtk_window_fullscreen(GTK_WINDOW(window_));
  } else {
    gtk_window_unfullscreen(GTK_WINDOW(window_));
  }
}

bool GTKWindow::is_bordered() const {
  return gtk_window_get_decorated(GTK_WINDOW(window_));
}

void GTKWindow::set_bordered(bool enabled) {
  if (is_fullscreen()) {
    // Don't screw with the borders if we're fullscreen.
    return;
  }

  gtk_window_set_decorated(GTK_WINDOW(window_), enabled);
}

void GTKWindow::set_cursor_visible(bool value) {
  if (is_cursor_visible_ == value) {
    return;
  }
  if (value) {
    // TODO(dougvj) Show and hide cursor
  } else {
  }
}

void GTKWindow::set_focus(bool value) {
  if (has_focus_ == value) {
    return;
  }
  if (window_) {
    if (value) {
      gtk_window_activate_focus(GTK_WINDOW(window_));
    } else {
      // TODO(dougvj) Check to see if we need to do something here to unset
      // the focus.
    }
  } else {
    has_focus_ = value;
  }
}

void GTKWindow::Resize(int32_t width, int32_t height) {
  gtk_window_resize(GTK_WINDOW(window_), width, height);
}

void GTKWindow::Resize(int32_t left, int32_t top, int32_t right,
                       int32_t bottom) {
  // TODO(dougvj) Verify that this is the desired behavior from this call
  gtk_window_move(GTK_WINDOW(window_), left, top);
  gtk_window_resize(GTK_WINDOW(window_), left - right, top - bottom);
}

void GTKWindow::OnResize(UIEvent* e) {
  int32_t width;
  int32_t height;
  gtk_window_get_size(GTK_WINDOW(window_), &width, &height);
  if (width != width_ || height != height_) {
    width_ = width;
    height_ = height;
    Layout();
  }
  super::OnResize(e);
}

void GTKWindow::Invalidate() {
  super::Invalidate();
  // TODO(dougvj) I am not sure what this is supposed to do
}

void GTKWindow::Close() {
  if (closing_) {
    return;
  }
  closing_ = true;
  Close();
  OnClose();
}

void GTKWindow::OnMainMenuChange() {
  // We need to store the old handle for detachment
  static GtkWidget* box = nullptr;
  auto main_menu = reinterpret_cast<GTKMenuItem*>(main_menu_.get());
  if (main_menu && !is_fullscreen()) {
    if (box) gtk_widget_destroy(box);
    GtkWidget* menu = main_menu->handle();
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(box), menu, FALSE, FALSE, 3);
    gtk_container_add(GTK_CONTAINER(window_), box);
  }
}

bool GTKWindow::HandleWindowOwnerChange(GdkEventOwnerChange* event) {
  if (event->type == GDK_OWNER_CHANGE) {
    if (event->reason == GDK_OWNER_CHANGE_DESTROY) {
      OnDestroy();
    } else if (event->reason == GDK_OWNER_CHANGE_CLOSE) {
      closing_ = true;
      Close();
      OnClose();
    }
    return true;
  }
  return false;
}

bool GTKWindow::HandleWindowResize(GdkEventConfigure* event) {
  if (event->type == GDK_CONFIGURE) {
    auto e = UIEvent(this);
    OnResize(&e);
    return true;
  }
  return false;
}

bool GTKWindow::HandleWindowVisibility(GdkEventVisibility* event) {
  // TODO(dougvj) The gdk docs say that this is deprecated because modern window
  // managers composite everything and nothing is truly hidden.
  if (event->type == GDK_VISIBILITY_NOTIFY) {
    if (event->state == GDK_VISIBILITY_UNOBSCURED) {
      auto e = UIEvent(this);
      OnVisible(&e);
    } else {
      auto e = UIEvent(this);
      OnHidden(&e);
    }
    return true;
  }
  return false;
}

bool GTKWindow::HandleWindowFocus(GdkEventFocus* event) {
  if (event->type == GDK_FOCUS_CHANGE) {
    if (!event->in) {
      has_focus_ = false;
      auto e = UIEvent(this);
      OnLostFocus(&e);
    } else {
      has_focus_ = true;
      auto e = UIEvent(this);
      OnGotFocus(&e);
    }
    return true;
  }
  return false;
}

bool GTKWindow::HandleMouse(GdkEventAny* event) {
  MouseEvent::Button button = MouseEvent::Button::kNone;
  int32_t dx = 0;
  int32_t dy = 0;
  int32_t x = 0;
  int32_t y = 0;
  switch (event->type) {
    default:
      // Double click/etc?
      return true;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE: {
      GdkEventButton* e = reinterpret_cast<GdkEventButton*>(event);
      switch (e->button) {
        case 1:
          button = MouseEvent::Button::kLeft;
          break;
        case 3:
          button = MouseEvent::Button::kRight;
          break;
        case 2:
          button = MouseEvent::Button::kMiddle;
          break;
        case 4:
          button = MouseEvent::Button::kX1;
          break;
        case 5:
          button = MouseEvent::Button::kX2;
          break;
      }
      x = e->x;
      y = e->y;
      break;
    }
    case GDK_MOTION_NOTIFY: {
      GdkEventMotion* e = reinterpret_cast<GdkEventMotion*>(event);
      x = e->x;
      y = e->y;
      break;
    }
    case GDK_SCROLL: {
      GdkEventScroll* e = reinterpret_cast<GdkEventScroll*>(event);
      x = e->x;
      y = e->y;
      dx = e->delta_x;
      dy = e->delta_y;
      break;
    }
  }

  auto e = MouseEvent(this, button, x, y, dx, dy);
  switch (event->type) {
    case GDK_BUTTON_PRESS:
      OnMouseDown(&e);
      break;
    case GDK_BUTTON_RELEASE:
      OnMouseUp(&e);
      break;
    case GDK_MOTION_NOTIFY:
      OnMouseMove(&e);
      break;
    case GDK_SCROLL:
      OnMouseWheel(&e);
      break;
    default:
      return false;
  }
  return e.is_handled();
}

static KeyEvent::Key MapGdkKeyToKeyEventKey(guint key) {
  switch (key) {
    case GDK_KEY_Escape:
      return KeyEvent::Key::kEsc;
    case GDK_KEY_F1:
      return KeyEvent::Key::kF1;
    case GDK_KEY_F2:
      return KeyEvent::Key::kF2;
    case GDK_KEY_F3:
      return KeyEvent::Key::kF3;
    case GDK_KEY_F4:
      return KeyEvent::Key::kF4;
    case GDK_KEY_F5:
      return KeyEvent::Key::kF5;
    case GDK_KEY_F6:
      return KeyEvent::Key::kF6;
    case GDK_KEY_F7:
      return KeyEvent::Key::kF7;
    case GDK_KEY_F8:
      return KeyEvent::Key::kF8;
    case GDK_KEY_F9:
      return KeyEvent::Key::kF9;
    case GDK_KEY_F10:
      return KeyEvent::Key::kF10;
    case GDK_KEY_F11:
      return KeyEvent::Key::kF11;
    case GDK_KEY_F12:
      return KeyEvent::Key::kF12;
    case GDK_KEY_asciitilde:
    case GDK_KEY_grave:
      return KeyEvent::Key::kTick;
    case GDK_KEY_1:
    case GDK_KEY_exclam:
      return KeyEvent::Key::k1;
    case GDK_KEY_2:
      return KeyEvent::Key::k2;
    case GDK_KEY_3:
      return KeyEvent::Key::k3;
    case GDK_KEY_4:
      return KeyEvent::Key::k4;
    case GDK_KEY_5:
      return KeyEvent::Key::k5;
    case GDK_KEY_6:
      return KeyEvent::Key::k6;
    case GDK_KEY_7:
      return KeyEvent::Key::k7;
    case GDK_KEY_8:
      return KeyEvent::Key::k8;
    case GDK_KEY_9:
      return KeyEvent::Key::k9;
    case GDK_KEY_0:
      return KeyEvent::Key::k0;
    case GDK_KEY_minus:
    case GDK_KEY_underscore:
      return KeyEvent::Key::kMinus;
    case GDK_KEY_equal:
    case GDK_KEY_plus:
      return KeyEvent::Key::kEquals;
    case GDK_KEY_BackSpace:
      return KeyEvent::Key::kBackspace;
    case GDK_KEY_Tab:
      return KeyEvent::Key::kTab;
    case GDK_KEY_q:
    case GDK_KEY_Q:
      return KeyEvent::Key::kQ;
    case GDK_KEY_w:
    case GDK_KEY_W:
      return KeyEvent::Key::kW;
    case GDK_KEY_e:
    case GDK_KEY_E:
      return KeyEvent::Key::kE;
    case GDK_KEY_r:
    case GDK_KEY_R:
      return KeyEvent::Key::kR;
    case GDK_KEY_t:
    case GDK_KEY_T:
      return KeyEvent::Key::kT;
    case GDK_KEY_y:
    case GDK_KEY_Y:
      return KeyEvent::Key::kY;
    case GDK_KEY_u:
    case GDK_KEY_U:
      return KeyEvent::Key::kU;
    case GDK_KEY_i:
    case GDK_KEY_I:
      return KeyEvent::Key::kI;
    case GDK_KEY_o:
    case GDK_KEY_O:
      return KeyEvent::Key::kO;
    case GDK_KEY_p:
    case GDK_KEY_P:
      return KeyEvent::Key::kP;
    case GDK_KEY_bracketleft:
    case GDK_KEY_braceleft:
      return KeyEvent::Key::kLeftBracket;
    case GDK_KEY_bracketright:
    case GDK_KEY_braceright:
      return KeyEvent::Key::kRightBracket;
    case GDK_KEY_backslash:
    case GDK_KEY_bar:
      return KeyEvent::Key::kBackSlash;
    case GDK_KEY_Caps_Lock:
      return KeyEvent::Key::kCapsLock;
    case GDK_KEY_a:
    case GDK_KEY_A:
      return KeyEvent::Key::kA;
    case GDK_KEY_s:
    case GDK_KEY_S:
      return KeyEvent::Key::kS;
    case GDK_KEY_d:
    case GDK_KEY_D:
      return KeyEvent::Key::kD;
    case GDK_KEY_f:
    case GDK_KEY_F:
      return KeyEvent::Key::kF;
    case GDK_KEY_g:
    case GDK_KEY_G:
      return KeyEvent::Key::kG;
    case GDK_KEY_h:
    case GDK_KEY_H:
      return KeyEvent::Key::kH;
    case GDK_KEY_j:
    case GDK_KEY_J:
      return KeyEvent::Key::kJ;
    case GDK_KEY_k:
    case GDK_KEY_K:
      return KeyEvent::Key::kK;
    case GDK_KEY_l:
    case GDK_KEY_L:
      return KeyEvent::Key::kL;
    case GDK_KEY_semicolon:
    case GDK_KEY_colon:
      return KeyEvent::Key::kSemiColon;
    case GDK_KEY_apostrophe:
    case GDK_KEY_quotedbl:
      return KeyEvent::Key::kQuote;
    case GDK_KEY_Return:
      return KeyEvent::Key::kEnter;
    case GDK_KEY_Shift_L:
      return KeyEvent::Key::kLeftShift;
    case GDK_KEY_z:
    case GDK_KEY_Z:
      return KeyEvent::Key::kZ;
    case GDK_KEY_x:
    case GDK_KEY_X:
      return KeyEvent::Key::kX;
    case GDK_KEY_c:
    case GDK_KEY_C:
      return KeyEvent::Key::kC;
    case GDK_KEY_v:
    case GDK_KEY_V:
      return KeyEvent::Key::kV;
    case GDK_KEY_b:
    case GDK_KEY_B:
      return KeyEvent::Key::kB;
    case GDK_KEY_n:
    case GDK_KEY_N:
      return KeyEvent::Key::kN;
    case GDK_KEY_m:
    case GDK_KEY_M:
      return KeyEvent::Key::kM;
    case GDK_KEY_less:
    case GDK_KEY_comma:
      return KeyEvent::Key::kComma;
    case GDK_KEY_greater:
    case GDK_KEY_period:
      return KeyEvent::Key::kPeriod;
    case GDK_KEY_slash:
    case GDK_KEY_question:
      return KeyEvent::Key::kSlash;
    case GDK_KEY_Shift_R:
      return KeyEvent::Key::kRightShift;
    case GDK_KEY_Control_L:
      return KeyEvent::Key::kLeftControl;
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
      return KeyEvent::Key::kSuper;
    case GDK_KEY_Alt_L:
      return KeyEvent::Key::kLeftAlt;
    case GDK_KEY_space:
      return KeyEvent::Key::kSpace;
    case GDK_KEY_Alt_R:
      return KeyEvent::Key::kRightAlt;
    case GDK_KEY_Control_R:
      return KeyEvent::Key::kRightControl;
    case GDK_KEY_Up:
      return KeyEvent::Key::kUp;
    case GDK_KEY_Down:
      return KeyEvent::Key::kDown;
    case GDK_KEY_Left:
      return KeyEvent::Key::kLeft;
    case GDK_KEY_Right:
      return KeyEvent::Key::kRight;
    case GDK_KEY_Insert:
      return KeyEvent::Key::kInsert;
    case GDK_KEY_Delete:
      return KeyEvent::Key::kDelete;
    case GDK_KEY_Home:
      return KeyEvent::Key::kHome;
    case GDK_KEY_End:
      return KeyEvent::Key::kEnd;
    case GDK_KEY_Page_Up:
      return KeyEvent::Key::kPageUp;
    case GDK_KEY_Page_Down:
      return KeyEvent::Key::kPageDown;
    case GDK_KEY_KP_Multiply:
      return KeyEvent::Key::kNpStar;
    case GDK_KEY_KP_Subtract:
      return KeyEvent::Key::kNpMinus;
    case GDK_KEY_KP_Add:
      return KeyEvent::Key::kNpPlus;
    case GDK_KEY_Pause:
      return KeyEvent::Key::kPause;
  }
  return KeyEvent::Key::kNone;
}

bool GTKWindow::HandleKeyboard(GdkEventKey* event) {
  unsigned int modifiers = event->state;
  bool shift_pressed = modifiers & GDK_SHIFT_MASK;
  bool ctrl_pressed = modifiers & GDK_CONTROL_MASK;
  bool alt_pressed = modifiers & GDK_META_MASK;
  bool super_pressed = modifiers & GDK_SUPER_MASK;
  KeyEvent::Key key = MapGdkKeyToKeyEventKey(event->keyval);
  uint32_t key_char = gdk_keyval_to_unicode(event->keyval);
  auto e = KeyEvent(this, key, event->keyval, key_char, 1,
                    event->type == GDK_KEY_RELEASE, shift_pressed, ctrl_pressed,
                    alt_pressed, super_pressed);
  switch (event->type) {
    case GDK_KEY_PRESS:
      OnKeyDown(&e);
      break;
    case GDK_KEY_RELEASE:
      OnKeyUp(&e);
      break;
    // TODO(dougvj) GDK doesn't have a KEY CHAR event, so we will have to
    // figure out its equivalent here to call  OnKeyChar(&e);
    default:
      return false;
  }
  return e.is_handled();
}

std::unique_ptr<ui::MenuItem> MenuItem::Create(Type type,
                                               const std::wstring& text,
                                               const std::wstring& hotkey,
                                               std::function<void()> callback) {
  return std::make_unique<GTKMenuItem>(type, text, hotkey, callback);
}

static void _menu_activate_callback(GtkWidget* menu, gpointer data) {
  auto fn = reinterpret_cast<FnWrapper*>(data);
  fn->Call();
  delete fn;
}

GTKMenuItem::GTKMenuItem(Type type, const std::wstring& text,
                         const std::wstring& hotkey,
                         std::function<void()> callback)
    : MenuItem(type, text, hotkey, std::move(callback)) {
  switch (type) {
    case MenuItem::Type::kNormal:
    default:
      menu_ = gtk_menu_bar_new();
      break;
    case MenuItem::Type::kPopup:
      menu_ = gtk_menu_item_new_with_label((gchar*)xe::to_string(text).c_str());
      break;
    case MenuItem::Type::kSeparator:
      menu_ = gtk_separator_menu_item_new();
      break;
    case MenuItem::Type::kString:
      auto full_name = text;
      if (!hotkey.empty()) {
        full_name += L"\t" + hotkey;
      }
      menu_ = gtk_menu_item_new_with_label(
          (gchar*)xe::to_string(full_name).c_str());
      break;
  }
  if (GTK_IS_MENU_ITEM(menu_))
    g_signal_connect(menu_, "activate", G_CALLBACK(_menu_activate_callback),
                     (gpointer) new FnWrapper(callback));
}

GTKMenuItem::~GTKMenuItem() {
  if (menu_) {
  }
}

void GTKMenuItem::OnChildAdded(MenuItem* generic_child_item) {
  auto child_item = static_cast<GTKMenuItem*>(generic_child_item);
  switch (child_item->type()) {
    case MenuItem::Type::kNormal:
      // Nothing special.
      break;
    case MenuItem::Type::kPopup:
      if (GTK_IS_MENU_ITEM(menu_)) {
        assert(gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_)) == nullptr);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_), child_item->handle());
      } else {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), child_item->handle());
      }
      break;
    case MenuItem::Type::kSeparator:
    case MenuItem::Type::kString:
      assert(GTK_IS_MENU_ITEM(menu_));
      // Get sub menu and if it doesn't exist create it
      GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_));
      if (submenu == nullptr) {
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_), submenu);
      }
      gtk_menu_shell_append(GTK_MENU_SHELL(submenu), child_item->handle());
      break;
  }
}

// TODO(dougvj)
void GTKMenuItem::OnChildRemoved(MenuItem* generic_child_item) {
  assert_always();
}

}  // namespace ui
}  // namespace xe
