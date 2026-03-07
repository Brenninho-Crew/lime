#include "SDLApplication.h"
#include "SDLGamepad.h"
#include "SDLJoystick.h"
#include <system/System.h>

#ifdef HX_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#ifdef ANDROID
#include <SDL_main.h>
#endif

namespace lime {

    // Static member initialization
    AutoGCRoot* Application::callback = nullptr;
    SDLApplication* SDLApplication::s_currentApplication = nullptr;

    // Constants
    static const int ANALOG_AXIS_DEAD_ZONE = 1000;
    static const double ANALOG_AXIS_MAX_VALUE = 32767.0;
    static const double ANALOG_AXIS_MIN_VALUE = -32768.0;
    static const double DEFAULT_FRAME_RATE = 60.0;

    // Static variables
    static std::map<int, std::map<int, int>> s_gamepadsAxisMap;
    static bool s_inBackground = false;
    static SDL_TimerID s_timerID = 0;
    static bool s_timerActive = false;
    static bool s_firstTime = true;

    // ============================================================================
    // Construction & Destruction
    // ============================================================================

    SDLApplication::SDLApplication() {
        InitializeSDL();
        InitializeMembers();
        InitializePlatformSpecific();
    }

    SDLApplication::~SDLApplication() {
        // Nothing to clean up here - Quit() handles SDL_Quit
    }

    // ============================================================================
    // Initialization Methods
    // ============================================================================

    void SDLApplication::InitializeSDL() {
        Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | 
                          SDL_INIT_TIMER | SDL_INIT_JOYSTICK;
        
        #if defined(LIME_MOJOAL) || defined(LIME_OPENALSOFT)
        initFlags |= SDL_INIT_AUDIO;
        #endif

        if (SDL_Init(initFlags) != 0) {
            fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN);
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        
        s_currentApplication = this;
    }

    void SDLApplication::InitializeMembers() {
        SetFrameRate(DEFAULT_FRAME_RATE);
        
        m_currentUpdate = 0;
        m_lastUpdate = 0;
        m_nextUpdate = 0;
        m_active = false;

        // Initialize event objects
        m_applicationEvent = ApplicationEvent();
        m_clipboardEvent = ClipboardEvent();
        m_dropEvent = DropEvent();
        m_gamepadEvent = GamepadEvent();
        m_joystickEvent = JoystickEvent();
        m_keyEvent = KeyEvent();
        m_mouseEvent = MouseEvent();
        m_renderEvent = RenderEvent();
        m_sensorEvent = SensorEvent();
        m_textEvent = TextEvent();
        m_touchEvent = TouchEvent();
        m_windowEvent = WindowEvent();
    }

    void SDLApplication::InitializePlatformSpecific() {
        SDLJoystick::Init();

        #ifdef HX_MACOS
        ChangeToResourcesDirectory();
        #endif
    }

    #ifdef HX_MACOS
    void SDLApplication::ChangeToResourcesDirectory() {
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
        if (!resourcesURL) return;

        char path[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8*)path, PATH_MAX)) {
            chdir(path);
        }
        
        CFRelease(resourcesURL);
    }
    #endif

    // ============================================================================
    // Application Lifecycle
    // ============================================================================

    int SDLApplication::Exec() {
        Init();

        #ifdef EMSCRIPTEN
        return RunEmscripten();
        #elif defined(IPHONE) || defined(ANDROID)
        return RunMobileLoop();
        #else
        return RunDesktopLoop();
        #endif
    }

    void SDLApplication::Init() {
        m_active = true;
        m_lastUpdate = SDL_GetTicks();
        m_nextUpdate = m_lastUpdate;
    }

    int SDLApplication::Quit() {
        m_applicationEvent.type = EventType::EXIT;
        ApplicationEvent::Dispatch(&m_applicationEvent);

        SDL_Quit();
        return 0;
    }

    // ============================================================================
    // Main Loop Implementations (Platform-Specific)
    // ============================================================================

    #ifdef EMSCRIPTEN
    int SDLApplication::RunEmscripten() {
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(UpdateFrameStatic, 0, 0);
        emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
        return 0;
    }
    #endif

    #if defined(IPHONE) || defined(ANDROID)
    int SDLApplication::RunMobileLoop() {
        // Mobile platforms use the system's event loop
        return 0;
    }
    #endif

    int SDLApplication::RunDesktopLoop() {
        while (m_active) {
            Update();
        }
        return Quit();
    }

    // ============================================================================
    // Update Methods
    // ============================================================================

    bool SDLApplication::Update() {
        SDL_Event event;

        #if !defined(IPHONE) && !defined(EMSCRIPTEN)
        if (m_active && (s_firstTime || WaitEvent(&event))) {
            s_firstTime = false;
            HandleEvent(&event);
            if (!m_active) return m_active;
        }
        #endif

        // Process all pending events
        while (SDL_PollEvent(&event)) {
            HandleEvent(&event);
            if (!m_active) return m_active;
        }

        m_currentUpdate = SDL_GetTicks();

        #if defined(IPHONE) || defined(EMSCRIPTEN)
        if (m_currentUpdate >= m_nextUpdate) {
            TriggerUpdateEvent();
        }
        #else
        HandleTimerBasedUpdate();
        #endif

        return m_active;
    }

    void SDLApplication::HandleTimerBasedUpdate() {
        if (m_currentUpdate >= m_nextUpdate) {
            if (s_timerActive) SDL_RemoveTimer(s_timerID);
            OnTimer(0, nullptr);
        } else if (!s_timerActive) {
            s_timerActive = true;
            s_timerID = SDL_AddTimer(m_nextUpdate - m_currentUpdate, OnTimer, nullptr);
        }
    }

    void SDLApplication::TriggerUpdateEvent() {
        if (s_inBackground) return;

        m_currentUpdate = SDL_GetTicks();
        m_applicationEvent.type = EventType::UPDATE;
        m_applicationEvent.deltaTime = m_currentUpdate - m_lastUpdate;
        m_lastUpdate = m_currentUpdate;

        m_nextUpdate += m_framePeriod;
        while (m_nextUpdate <= m_currentUpdate) {
            m_nextUpdate += m_framePeriod;
        }

        ApplicationEvent::Dispatch(&m_applicationEvent);
        RenderEvent::Dispatch(&m_renderEvent);
    }

    // ============================================================================
    // Static Update Callbacks
    // ============================================================================

    void SDLApplication::UpdateFrameStatic() {
        #ifdef EMSCRIPTEN
        System::GCTryExitBlocking();
        #endif

        if (s_currentApplication) {
            s_currentApplication->Update();
        }

        #ifdef EMSCRIPTEN
        System::GCTryEnterBlocking();
        #endif
    }

    void SDLApplication::UpdateFrameStatic(void*) {
        UpdateFrameStatic();
    }

    Uint32 SDLApplication::OnTimer(Uint32 interval, void*) {
        SDL_Event event;
        SDL_UserEvent userevent;
        userevent.type = SDL_USEREVENT;
        userevent.code = 0;
        userevent.data1 = nullptr;
        userevent.data2 = nullptr;
        
        event.type = SDL_USEREVENT;
        event.user = userevent;

        s_timerActive = false;
        s_timerID = 0;

        SDL_PushEvent(&event);
        return 0;
    }

    // ============================================================================
    // Event Handling (Main Dispatcher)
    // ============================================================================

    void SDLApplication::HandleEvent(SDL_Event* event) {
        #if defined(IPHONE) || defined(EMSCRIPTEN)
        int top = 0;
        gc_set_top_of_stack(&top, false);
        #endif

        switch (event->type) {
            case SDL_USEREVENT:
                TriggerUpdateEvent();
                break;

            case SDL_APP_WILLENTERBACKGROUND:
                HandleAppWillEnterBackground();
                break;

            case SDL_APP_WILLENTERFOREGROUND:
                // Nothing needed here
                break;

            case SDL_APP_DIDENTERFOREGROUND:
                HandleAppDidEnterForeground();
                break;

            case SDL_CLIPBOARDUPDATE:
                ProcessClipboardEvent(event);
                break;

            case SDL_CONTROLLERAXISMOTION:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
                ProcessGamepadEvent(event);
                break;

            case SDL_DROPFILE:
                ProcessDropEvent(event);
                break;

            case SDL_FINGERMOTION:
            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
                ProcessTouchEvent(event);
                break;

            case SDL_JOYAXISMOTION:
                ProcessJoystickOrSensorEvent(event);
                break;

            case SDL_JOYBALLMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_JOYHATMOTION:
            case SDL_JOYDEVICEADDED:
            case SDL_JOYDEVICEREMOVED:
                ProcessJoystickEvent(event);
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                ProcessKeyEvent(event);
                break;

            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                ProcessMouseEvent(event);
                break;

            #ifndef EMSCRIPTEN
            case SDL_RENDER_DEVICE_RESET:
                ProcessRenderDeviceReset();
                break;
            #endif

            case SDL_TEXTINPUT:
            case SDL_TEXTEDITING:
                ProcessTextEvent(event);
                break;

            case SDL_WINDOWEVENT:
                ProcessWindowEvent(event);
                break;

            case SDL_QUIT:
                m_active = false;
                break;

            default:
                // Ignore unhandled events
                break;
        }
    }

    // ============================================================================
    // App Background/Foreground Handlers
    // ============================================================================

    void SDLApplication::HandleAppWillEnterBackground() {
        s_inBackground = true;

        m_windowEvent.type = WindowEventType::WINDOW_DEACTIVATE;
        WindowEvent::Dispatch(&m_windowEvent);
    }

    void SDLApplication::HandleAppDidEnterForeground() {
        m_windowEvent.type = WindowEventType::WINDOW_ACTIVATE;
        WindowEvent::Dispatch(&m_windowEvent);

        s_inBackground = false;
    }

    // ============================================================================
    // Processors for Specific Event Types
    // ============================================================================

    void SDLApplication::ProcessClipboardEvent(SDL_Event* event) {
        if (!ClipboardEvent::callback) return;

        m_clipboardEvent.type = ClipboardEventType::CLIPBOARD_UPDATE;
        ClipboardEvent::Dispatch(&m_clipboardEvent);
    }

    void SDLApplication::ProcessDropEvent(SDL_Event* event) {
        if (!DropEvent::callback) return;

        m_dropEvent.type = DropEventType::DROP_FILE;
        m_dropEvent.file = reinterpret_cast<vbyte*>(event->drop.file);

        DropEvent::Dispatch(&m_dropEvent);
        SDL_free(const_cast<char*>(event->drop.file));
    }

    void SDLApplication::ProcessGamepadEvent(SDL_Event* event) {
        if (!GamepadEvent::callback) return;

        switch (event->type) {
            case SDL_CONTROLLERAXISMOTION:
                ProcessGamepadAxisMotion(event);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                ProcessGamepadButtonDown(event);
                break;

            case SDL_CONTROLLERBUTTONUP:
                ProcessGamepadButtonUp(event);
                break;

            case SDL_CONTROLLERDEVICEADDED:
                ProcessGamepadDeviceAdded(event);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                ProcessGamepadDeviceRemoved(event);
                break;
        }
    }

    void SDLApplication::ProcessGamepadAxisMotion(SDL_Event* event) {
        int gamepadId = event->caxis.which;
        int axis = event->caxis.axis;
        int value = event->caxis.value;

        // Skip if value hasn't changed
        auto& axisMap = s_gamepadsAxisMap[gamepadId];
        if (!axisMap.empty() && axisMap[axis] == value) {
            return;
        }

        m_gamepadEvent.type = GamepadEventType::GAMEPAD_AXIS_MOVE;
        m_gamepadEvent.axis = axis;
        m_gamepadEvent.id = gamepadId;

        // Handle dead zone
        if (value > -ANALOG_AXIS_DEAD_ZONE && value < ANALOG_AXIS_DEAD_ZONE) {
            if (axisMap[axis] != 0) {
                axisMap[axis] = 0;
                m_gamepadEvent.axisValue = 0.0;
                GamepadEvent::Dispatch(&m_gamepadEvent);
            }
            return;
        }

        axisMap[axis] = value;
        m_gamepadEvent.axisValue = NormalizeAxisValue(value);
        GamepadEvent::Dispatch(&m_gamepadEvent);
    }

    double SDLApplication::NormalizeAxisValue(int value) const {
        return value / (value > 0 ? ANALOG_AXIS_MAX_VALUE : ANALOG_AXIS_MIN_VALUE);
    }

    void SDLApplication::ProcessGamepadButtonDown(SDL_Event* event) {
        m_gamepadEvent.type = GamepadEventType::GAMEPAD_BUTTON_DOWN;
        m_gamepadEvent.button = event->cbutton.button;
        m_gamepadEvent.id = event->cbutton.which;
        GamepadEvent::Dispatch(&m_gamepadEvent);
    }

    void SDLApplication::ProcessGamepadButtonUp(SDL_Event* event) {
        m_gamepadEvent.type = GamepadEventType::GAMEPAD_BUTTON_UP;
        m_gamepadEvent.button = event->cbutton.button;
        m_gamepadEvent.id = event->cbutton.which;
        GamepadEvent::Dispatch(&m_gamepadEvent);
    }

    void SDLApplication::ProcessGamepadDeviceAdded(SDL_Event* event) {
        if (SDLGamepad::Connect(event->cdevice.which)) {
            m_gamepadEvent.type = GamepadEventType::GAMEPAD_CONNECT;
            m_gamepadEvent.id = SDLGamepad::GetInstanceID(event->cdevice.which);
            GamepadEvent::Dispatch(&m_gamepadEvent);
        }
    }

    void SDLApplication::ProcessGamepadDeviceRemoved(SDL_Event* event) {
        m_gamepadEvent.type = GamepadEventType::GAMEPAD_DISCONNECT;
        m_gamepadEvent.id = event->cdevice.which;
        GamepadEvent::Dispatch(&m_gamepadEvent);
        SDLGamepad::Disconnect(event->cdevice.which);
    }

    void SDLApplication::ProcessJoystickOrSensorEvent(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jaxis.which)) {
            ProcessSensorEvent(event);
        } else {
            ProcessJoystickEvent(event);
        }
    }

    void SDLApplication::ProcessJoystickEvent(SDL_Event* event) {
        if (!JoystickEvent::callback) return;

        switch (event->type) {
            case SDL_JOYAXISMOTION:
                ProcessJoystickAxisMotion(event);
                break;

            case SDL_JOYBALLMOTION:
                ProcessJoystickBallMotion(event);
                break;

            case SDL_JOYBUTTONDOWN:
                ProcessJoystickButtonDown(event);
                break;

            case SDL_JOYBUTTONUP:
                ProcessJoystickButtonUp(event);
                break;

            case SDL_JOYHATMOTION:
                ProcessJoystickHatMotion(event);
                break;

            case SDL_JOYDEVICEADDED:
                ProcessJoystickDeviceAdded(event);
                break;

            case SDL_JOYDEVICEREMOVED:
                ProcessJoystickDeviceRemoved(event);
                break;
        }
    }

    void SDLApplication::ProcessJoystickAxisMotion(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jaxis.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_AXIS_MOVE;
        m_joystickEvent.index = event->jaxis.axis;
        m_joystickEvent.x = NormalizeAxisValue(event->jaxis.value);
        m_joystickEvent.id = event->jaxis.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
    }

    void SDLApplication::ProcessJoystickBallMotion(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jball.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_TRACKBALL_MOVE;
        m_joystickEvent.index = event->jball.ball;
        m_joystickEvent.x = event->jball.xrel / ANALOG_AXIS_MAX_VALUE;
        m_joystickEvent.y = event->jball.yrel / ANALOG_AXIS_MAX_VALUE;
        m_joystickEvent.id = event->jball.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
    }

    void SDLApplication::ProcessJoystickButtonDown(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jbutton.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_BUTTON_DOWN;
        m_joystickEvent.index = event->jbutton.button;
        m_joystickEvent.id = event->jbutton.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
    }

    void SDLApplication::ProcessJoystickButtonUp(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jbutton.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_BUTTON_UP;
        m_joystickEvent.index = event->jbutton.button;
        m_joystickEvent.id = event->jbutton.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
    }

    void SDLApplication::ProcessJoystickHatMotion(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jhat.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_HAT_MOVE;
        m_joystickEvent.index = event->jhat.hat;
        m_joystickEvent.eventValue = event->jhat.value;
        m_joystickEvent.id = event->jhat.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
    }

    void SDLApplication::ProcessJoystickDeviceAdded(SDL_Event* event) {
        if (SDLJoystick::Connect(event->jdevice.which)) {
            m_joystickEvent.type = JoystickEventType::JOYSTICK_CONNECT;
            m_joystickEvent.id = SDLJoystick::GetInstanceID(event->jdevice.which);
            JoystickEvent::Dispatch(&m_joystickEvent);
        }
    }

    void SDLApplication::ProcessJoystickDeviceRemoved(SDL_Event* event) {
        if (SDLJoystick::IsAccelerometer(event->jdevice.which)) return;

        m_joystickEvent.type = JoystickEventType::JOYSTICK_DISCONNECT;
        m_joystickEvent.id = event->jdevice.which;

        JoystickEvent::Dispatch(&m_joystickEvent);
        SDLJoystick::Disconnect(event->jdevice.which);
    }

    void SDLApplication::ProcessKeyEvent(SDL_Event* event) {
        if (!KeyEvent::callback) return;

        switch (event->type) {
            case SDL_KEYDOWN:
                m_keyEvent.type = KeyEventType::KEY_DOWN;
                break;
            case SDL_KEYUP:
                m_keyEvent.type = KeyEventType::KEY_UP;
                break;
            default:
                return;
        }

        m_keyEvent.keyCode = event->key.keysym.sym;
        m_keyEvent.modifier = event->key.keysym.mod;
        m_keyEvent.windowID = event->key.windowID;

        if (m_keyEvent.type == KeyEventType::KEY_DOWN) {
            UpdateKeyModifiersFromKeyCode();
        }

        KeyEvent::Dispatch(&m_keyEvent);
    }

    void SDLApplication::UpdateKeyModifiersFromKeyCode() {
        if (m_keyEvent.keyCode == SDLK_CAPSLOCK) m_keyEvent.modifier |= KMOD_CAPS;
        if (m_keyEvent.keyCode == SDLK_LALT) m_keyEvent.modifier |= KMOD_LALT;
        if (m_keyEvent.keyCode == SDLK_LCTRL) m_keyEvent.modifier |= KMOD_LCTRL;
        if (m_keyEvent.keyCode == SDLK_LGUI) m_keyEvent.modifier |= KMOD_LGUI;
        if (m_keyEvent.keyCode == SDLK_LSHIFT) m_keyEvent.modifier |= KMOD_LSHIFT;
        if (m_keyEvent.keyCode == SDLK_MODE) m_keyEvent.modifier |= KMOD_MODE;
        if (m_keyEvent.keyCode == SDLK_NUMLOCKCLEAR) m_keyEvent.modifier |= KMOD_NUM;
        if (m_keyEvent.keyCode == SDLK_RALT) m_keyEvent.modifier |= KMOD_RALT;
        if (m_keyEvent.keyCode == SDLK_RCTRL) m_keyEvent.modifier |= KMOD_RCTRL;
        if (m_keyEvent.keyCode == SDLK_RGUI) m_keyEvent.modifier |= KMOD_RGUI;
        if (m_keyEvent.keyCode == SDLK_RSHIFT) m_keyEvent.modifier |= KMOD_RSHIFT;
    }

    void SDLApplication::ProcessMouseEvent(SDL_Event* event) {
        if (!MouseEvent::callback) return;

        switch (event->type) {
            case SDL_MOUSEMOTION:
                ProcessMouseMotion(event);
                break;

            case SDL_MOUSEBUTTONDOWN:
                ProcessMouseButtonDown(event);
                break;

            case SDL_MOUSEBUTTONUP:
                ProcessMouseButtonUp(event);
                break;

            case SDL_MOUSEWHEEL:
                ProcessMouseWheel(event);
                break;

            default:
                return;
        }

        m_mouseEvent.windowID = event->button.windowID;
        MouseEvent::Dispatch(&m_mouseEvent);
    }

    void SDLApplication::ProcessMouseMotion(SDL_Event* event) {
        m_mouseEvent.type = MouseEventType::MOUSE_MOVE;
        m_mouseEvent.x = event->motion.x;
        m_mouseEvent.y = event->motion.y;
        m_mouseEvent.movementX = event->motion.xrel;
        m_mouseEvent.movementY = event->motion.yrel;
    }

    void SDLApplication::ProcessMouseButtonDown(SDL_Event* event) {
        SDL_CaptureMouse(SDL_TRUE);

        m_mouseEvent.type = MouseEventType::MOUSE_DOWN;
        m_mouseEvent.button = event->button.button - 1;
        m_mouseEvent.x = event->button.x;
        m_mouseEvent.y = event->button.y;
        m_mouseEvent.clickCount = event->button.clicks;
    }

    void SDLApplication::ProcessMouseButtonUp(SDL_Event* event) {
        SDL_CaptureMouse(SDL_FALSE);

        m_mouseEvent.type = MouseEventType::MOUSE_UP;
        m_mouseEvent.button = event->button.button - 1;
        m_mouseEvent.x = event->button.x;
        m_mouseEvent.y = event->button.y;
        m_mouseEvent.clickCount = event->button.clicks;
    }

    void SDLApplication::ProcessMouseWheel(SDL_Event* event) {
        m_mouseEvent.type = MouseEventType::MOUSE_WHEEL;

        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            m_mouseEvent.x = -event->wheel.x;
            m_mouseEvent.y = -event->wheel.y;
        } else {
            m_mouseEvent.x = event->wheel.x;
            m_mouseEvent.y = event->wheel.y;
        }
    }

    void SDLApplication::ProcessSensorEvent(SDL_Event* event) {
        if (!SensorEvent::callback) return;

        double value = event->jaxis.value / ANALOG_AXIS_MAX_VALUE;

        switch (event->jaxis.axis) {
            case 0: m_sensorEvent.x = value; break;
            case 1: m_sensorEvent.y = value; break;
            case 2: m_sensorEvent.z = value; break;
            default: return;
        }

        SensorEvent::Dispatch(&m_sensorEvent);
    }

    void SDLApplication::ProcessTextEvent(SDL_Event* event) {
        if (!TextEvent::callback) return;

        switch (event->type) {
            case SDL_TEXTINPUT:
                m_textEvent.type = TextEventType::TEXT_INPUT;
                break;

            case SDL_TEXTEDITING:
                m_textEvent.type = TextEventType::TEXT_EDIT;
                m_textEvent.start = event->edit.start;
                m_textEvent.length = event->edit.length;
                break;

            default:
                return;
        }

        // Free previous text if any
        if (m_textEvent.text) {
            free(m_textEvent.text);
            m_textEvent.text = nullptr;
        }

        // Copy new text
        size_t textLength = strlen(event->text.text);
        m_textEvent.text = static_cast<vbyte*>(malloc(textLength + 1));
        if (m_textEvent.text) {
            strcpy(reinterpret_cast<char*>(m_textEvent.text), event->text.text);
        }

        m_textEvent.windowID = event->text.windowID;
        TextEvent::Dispatch(&m_textEvent);
    }

    void SDLApplication::ProcessTouchEvent(SDL_Event* event) {
        if (!TouchEvent::callback) return;

        switch (event->type) {
            case SDL_FINGERMOTION:
                m_touchEvent.type = TouchEventType::TOUCH_MOVE;
                break;

            case SDL_FINGERDOWN:
                m_touchEvent.type = TouchEventType::TOUCH_START;
                break;

            case SDL_FINGERUP:
                m_touchEvent.type = TouchEventType::TOUCH_END;
                break;

            default:
                return;
        }

        m_touchEvent.x = event->tfinger.x;
        m_touchEvent.y = event->tfinger.y;
        m_touchEvent.id = event->tfinger.fingerId;
        m_touchEvent.dx = event->tfinger.dx;
        m_touchEvent.dy = event->tfinger.dy;
        m_touchEvent.pressure = event->tfinger.pressure;
        m_touchEvent.device = event->tfinger.touchId;

        TouchEvent::Dispatch(&m_touchEvent);
    }

    void SDLApplication::ProcessWindowEvent(SDL_Event* event) {
        if (!WindowEvent::callback) return;

        switch (event->window.event) {
            case SDL_WINDOWEVENT_SHOWN:
                m_windowEvent.type = WindowEventType::WINDOW_SHOW;
                break;

            case SDL_WINDOWEVENT_CLOSE:
                m_windowEvent.type = WindowEventType::WINDOW_CLOSE;
                break;

            case SDL_WINDOWEVENT_HIDDEN:
                m_windowEvent.type = WindowEventType::WINDOW_HIDE;
                break;

            case SDL_WINDOWEVENT_ENTER:
                m_windowEvent.type = WindowEventType::WINDOW_ENTER;
                break;

            case SDL_WINDOWEVENT_FOCUS_GAINED:
                m_windowEvent.type = WindowEventType::WINDOW_FOCUS_IN;
                break;

            case SDL_WINDOWEVENT_FOCUS_LOST:
                m_windowEvent.type = WindowEventType::WINDOW_FOCUS_OUT;
                break;

            case SDL_WINDOWEVENT_LEAVE:
                m_windowEvent.type = WindowEventType::WINDOW_LEAVE;
                break;

            case SDL_WINDOWEVENT_MAXIMIZED:
                m_windowEvent.type = WindowEventType::WINDOW_MAXIMIZE;
                break;

            case SDL_WINDOWEVENT_MINIMIZED:
                m_windowEvent.type = WindowEventType::WINDOW_MINIMIZE;
                break;

            case SDL_WINDOWEVENT_EXPOSED:
                m_windowEvent.type = WindowEventType::WINDOW_EXPOSE;
                break;

            case SDL_WINDOWEVENT_MOVED:
                m_windowEvent.type = WindowEventType::WINDOW_MOVE;
                m_windowEvent.x = event->window.data1;
                m_windowEvent.y = event->window.data2;
                break;

            case SDL_WINDOWEVENT_SIZE_CHANGED:
                m_windowEvent.type = WindowEventType::WINDOW_RESIZE;
                m_windowEvent.width = event->window.data1;
                m_windowEvent.height = event->window.data2;
                break;

            case SDL_WINDOWEVENT_RESTORED:
                m_windowEvent.type = WindowEventType::WINDOW_RESTORE;
                break;

            default:
                return;
        }

        m_windowEvent.windowID = event->window.windowID;
        WindowEvent::Dispatch(&m_windowEvent);

        // Handle close event specially
        if (event->window.event == SDL_WINDOWEVENT_CLOSE) {
            HandlePostWindowClose();
        }
    }

    void SDLApplication::HandlePostWindowClose() {
        // Check for QUIT event to avoid double-handling
        SDL_Event event;
        if (SDL_PollEvent(&event)) {
            if (event.type != SDL_QUIT) {
                HandleEvent(&event);
            }
        }
    }

    #ifndef EMSCRIPTEN
    void SDLApplication::ProcessRenderDeviceReset() {
        m_renderEvent.type = RenderEventType::RENDER_CONTEXT_LOST;
        RenderEvent::Dispatch(&m_renderEvent);

        m_renderEvent.type = RenderEventType::RENDER_CONTEXT_RESTORED;
        RenderEvent::Dispatch(&m_renderEvent);

        m_renderEvent.type = RenderEventType::RENDER;
    }
    #endif

    // ============================================================================
    // Window Management
    // ============================================================================

    void SDLApplication::RegisterWindow(SDLWindow* window) {
        #ifdef IPHONE
        if (window && window->sdlWindow) {
            SDL_iPhoneSetAnimationCallback(window->sdlWindow, 1, UpdateFrameStatic, nullptr);
        }
        #endif
    }

    // ============================================================================
    // Frame Rate Control
    // ============================================================================

    void SDLApplication::SetFrameRate(double frameRate) {
        if (frameRate > 0) {
            m_framePeriod = 1000.0 / frameRate;
        } else {
            m_framePeriod = 1000.0; // 1 FPS minimum
        }
    }

    // ============================================================================
    // Event Waiting (Platform-Specific)
    // ============================================================================

    int SDLApplication::WaitEvent(SDL_Event* event) {
        #if defined(HX_MACOS) || defined(ANDROID)
        return WaitEventBlocking(event);
        #else
        return WaitEventWithGC(event);
        #endif
    }

    int SDLApplication::WaitEventBlocking(SDL_Event* event) {
        System::GCEnterBlocking();
        int result = SDL_WaitEvent(event);
        System::GCExitBlocking();
        return result;
    }

    int SDLApplication::WaitEventWithGC(SDL_Event* event) {
        bool isBlocking = false;

        while (true) {
            SDL_PumpEvents();

            int result = SDL_PeepEvents(event, 1, SDL_GETEVENT, 
                                        SDL_FIRSTEVENT, SDL_LASTEVENT);

            switch (result) {
                case -1: // Error
                    if (isBlocking) System::GCExitBlocking();
                    return 0;

                case 1: // Event found
                    if (isBlocking) System::GCExitBlocking();
                    return 1;

                default: // No events
                    if (!isBlocking) {
                        System::GCEnterBlocking();
                        isBlocking = true;
                    }
                    SDL_Delay(1);
                    break;
            }
        }
    }

    // ============================================================================
    // Application Factory
    // ============================================================================

    Application* CreateApplication() {
        return new SDLApplication();
    }

} // namespace lime

// ============================================================================
// Android SDL Main Entry Point
// ============================================================================

#ifdef ANDROID
int SDL_main(int argc, char* argv[]) {
    return 0;
}
#endif