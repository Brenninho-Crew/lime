package lime.app;

import lime.graphics.RenderContext;
import lime.system.System;
import lime.ui.Gamepad;
import lime.ui.GamepadAxis;
import lime.ui.GamepadButton;
import lime.ui.Joystick;
import lime.ui.JoystickHatPosition;
import lime.ui.KeyCode;
import lime.ui.KeyModifier;
import lime.ui.MouseButton;
import lime.ui.MouseWheelMode;
import lime.ui.Touch;
import lime.ui.Window;
import lime.ui.WindowAttributes;
import lime.utils.Preloader;

/**
 * The Application class forms the foundation for most Lime projects.
 * 
 * It is common to extend this class in a main class. You can then override
 * "on" functions to handle standard events that are relevant to your application.
 * 
 * The Application manages windows, input devices, modules, and the main event loop.
 */
@:access(lime.ui.Window)
#if !lime_debug
@:fileXml('tags="haxe,release"')
@:noDebug
#end
class Application extends Module 
{
    // ============================================================================
    // Static Properties
    // ============================================================================
    
    /**
     * The current Application instance that is executing.
     * Only one application can be active at a time.
     */
    public static var current(default, null):Application;
    
    // ============================================================================
    // Public Properties
    // ============================================================================
    
    /**
     * Meta-data values for the application, such as version, package name, or build configuration.
     */
    public var meta:Map<String, String>;
    
    /**
     * A list of currently attached Module instances.
     * Modules can extend the functionality of the application.
     */
    public var modules(default, null):Array<IModule>;
    
    /**
     * The Preloader for the current Application.
     * Handles asset loading progress and completion.
     */
    public var preloader(get, never):Preloader;
    
    /**
     * The primary Window associated with this Application.
     * If there are multiple windows, this returns the first one created.
     */
    public var window(get, never):Window;
    
    /**
     * A list of all active Window instances associated with this Application.
     */
    public var windows(get, never):Array<Window>;
    
    // ============================================================================
    // Events
    // ============================================================================
    
    /**
     * Dispatched each frame before rendering.
     * @param deltaTime Time in milliseconds since the last update
     */
    public var onUpdate = new Event<Int->Void>();
    
    /**
     * Dispatched when a new window has been created by this application.
     * @param window The newly created window
     */
    public var onCreateWindow = new Event<Window->Void>();
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    @:noCompletion private var __backend:ApplicationBackend;
    @:noCompletion private var __preloader:Preloader;
    @:noCompletion private var __primaryWindow:Window;
    @:noCompletion private var __windowById:Map<Int, Window>;
    @:noCompletion private var __windows:Array<Window>;
    
    // ============================================================================
    // Static Initialization
    // ============================================================================
    
    private static function __init__():Void 
    {
        // Force initialization of backend
        var _init = ApplicationBackend;
        
        #if commonjs
        var prototype = untyped Application.prototype;
        untyped Object.defineProperties(prototype, {
            "preloader": { get: prototype.get_preloader },
            "window": { get: prototype.get_window },
            "windows": { get: prototype.get_windows }
        });
        #end
    }
    
    // ============================================================================
    // Constructor
    // ============================================================================
    
    /**
     * Creates a new Application instance.
     * If no application currently exists, this becomes the current application.
     */
    public function new() 
    {
        super();
        
        if (Application.current == null) 
        {
            Application.current = this;
        }
        
        meta = new Map();
        modules = [];
        __windowById = new Map();
        __windows = [];
        
        __backend = new ApplicationBackend(this);
        __preloader = new Preloader();
        
        __registerLimeModule(this);
        __setupPreloaderEvents();
    }
    
    // ============================================================================
    // Public API
    // ============================================================================
    
    /**
     * Adds a new module to the Application.
     * @param module The module to add
     */
    public function addModule(module:IModule):Void 
    {
        if (module == null) 
        {
            throw new ArgumentError("Module cannot be null");
        }
        
        module.__registerLimeModule(this);
        modules.push(module);
    }
    
    /**
     * Creates a new Window and adds it to the Application.
     * @param attributes Initialization parameters for the window
     * @return The newly created window, or null if creation failed
     */
    public function createWindow(attributes:WindowAttributes):Null<Window> 
    {
        var window = __createWindow(attributes);
        
        if (window != null) 
        {
            __addWindow(window);
        }
        
        return window;
    }
    
    /**
     * Executes the Application.
     * On native platforms, this method blocks until the application exits.
     * On other platforms (HTML5, Flash), it returns immediately.
     * 
     * @return An exit code (0 for success, non-zero for errors)
     */
    public function exec():Int 
    {
        Application.current = this;
        return __backend.exec();
    }
    
    /**
     * Removes a module from the Application.
     * @param module The module to remove
     */
    public function removeModule(module:IModule):Void 
    {
        if (module != null) 
        {
            module.__unregisterLimeModule(this);
            modules.remove(module);
        }
    }
    
    // ============================================================================
    // Event Handlers (Override these in subclasses)
    // ============================================================================
    
    // --- Gamepad Events ---
    
    /**
     * Called when a gamepad axis moves.
     * @param gamepad The gamepad that triggered the event
     * @param axis The axis that moved
     * @param value The axis value (normalized between -1.0 and 1.0)
     */
    public function onGamepadAxisMove(gamepad:Gamepad, axis:GamepadAxis, value:Float):Void {}
    
    /**
     * Called when a gamepad button is pressed.
     * @param gamepad The gamepad that triggered the event
     * @param button The button that was pressed
     */
    public function onGamepadButtonDown(gamepad:Gamepad, button:GamepadButton):Void {}
    
    /**
     * Called when a gamepad button is released.
     * @param gamepad The gamepad that triggered the event
     * @param button The button that was released
     */
    public function onGamepadButtonUp(gamepad:Gamepad, button:GamepadButton):Void {}
    
    /**
     * Called when a gamepad is connected.
     * @param gamepad The gamepad that was connected
     */
    public function onGamepadConnect(gamepad:Gamepad):Void {}
    
    /**
     * Called when a gamepad is disconnected.
     * @param gamepad The gamepad that was disconnected
     */
    public function onGamepadDisconnect(gamepad:Gamepad):Void {}
    
    // --- Joystick Events ---
    
    /**
     * Called when a joystick axis moves.
     * @param joystick The joystick that triggered the event
     * @param axis The axis index that moved
     * @param value The axis value (normalized between -1.0 and 1.0)
     */
    public function onJoystickAxisMove(joystick:Joystick, axis:Int, value:Float):Void {}
    
    /**
     * Called when a joystick button is pressed.
     * @param joystick The joystick that triggered the event
     * @param button The button index that was pressed
     */
    public function onJoystickButtonDown(joystick:Joystick, button:Int):Void {}
    
    /**
     * Called when a joystick button is released.
     * @param joystick The joystick that triggered the event
     * @param button The button index that was released
     */
    public function onJoystickButtonUp(joystick:Joystick, button:Int):Void {}
    
    /**
     * Called when a joystick is connected.
     * @param joystick The joystick that was connected
     */
    public function onJoystickConnect(joystick:Joystick):Void {}
    
    /**
     * Called when a joystick is disconnected.
     * @param joystick The joystick that was disconnected
     */
    public function onJoystickDisconnect(joystick:Joystick):Void {}
    
    /**
     * Called when a joystick hat moves.
     * @param joystick The joystick that triggered the event
     * @param hat The hat index that moved
     * @param position The current hat position
     */
    public function onJoystickHatMove(joystick:Joystick, hat:Int, position:JoystickHatPosition):Void {}
    
    /**
     * Called when a joystick trackball moves.
     * @param joystick The joystick that triggered the event
     * @param trackball The trackball index that moved
     * @param x The X movement (normalized between -1.0 and 1.0)
     * @param y The Y movement (normalized between -1.0 and 1.0)
     */
    public function onJoystickTrackballMove(joystick:Joystick, trackball:Int, x:Float, y:Float):Void {}
    
    // --- Keyboard Events ---
    
    /**
     * Called when a key is pressed on the primary window.
     * @param keyCode The code of the pressed key
     * @param modifier The modifier keys state (Shift, Ctrl, Alt, etc.)
     */
    public function onKeyDown(keyCode:KeyCode, modifier:KeyModifier):Void {}
    
    /**
     * Called when a key is released on the primary window.
     * @param keyCode The code of the released key
     * @param modifier The modifier keys state (Shift, Ctrl, Alt, etc.)
     */
    public function onKeyUp(keyCode:KeyCode, modifier:KeyModifier):Void {}
    
    // --- Module Events ---
    
    /**
     * Called when the module is exiting.
     * @param code The exit code
     */
    public function onModuleExit(code:Int):Void {}
    
    // --- Mouse Events ---
    
    /**
     * Called when a mouse button is pressed on the primary window.
     * @param x The X coordinate of the mouse
     * @param y The Y coordinate of the mouse
     * @param button The button that was pressed
     */
    public function onMouseDown(x:Float, y:Float, button:MouseButton):Void {}
    
    /**
     * Called when the mouse moves on the primary window.
     * @param x The X coordinate of the mouse
     * @param y The Y coordinate of the mouse
     */
    public function onMouseMove(x:Float, y:Float):Void {}
    
    /**
     * Called when the mouse moves relative to its previous position.
     * @param x The relative X movement
     * @param y The relative Y movement
     */
    public function onMouseMoveRelative(x:Float, y:Float):Void {}
    
    /**
     * Called when a mouse button is released on the primary window.
     * @param x The X coordinate of the mouse
     * @param y The Y coordinate of the mouse
     * @param button The button that was released
     */
    public function onMouseUp(x:Float, y:Float, button:MouseButton):Void {}
    
    /**
     * Called when the mouse wheel is scrolled on the primary window.
     * @param deltaX The horizontal scroll amount
     * @param deltaY The vertical scroll amount
     * @param deltaMode The units of measurement for the deltas
     */
    public function onMouseWheel(deltaX:Float, deltaY:Float, deltaMode:MouseWheelMode):Void {}
    
    // --- Preloader Events ---
    
    /**
     * Called when preloading is complete.
     */
    public function onPreloadComplete():Void {}
    
    /**
     * Called during preloading to report progress.
     * @param loaded The number of items loaded
     * @param total The total number of items to load
     */
    public function onPreloadProgress(loaded:Int, total:Int):Void {}
    
    // --- Render Events ---
    
    /**
     * Called when the render context is lost on the primary window.
     * Resources may need to be recreated when the context is restored.
     */
    public function onRenderContextLost():Void {}
    
    /**
     * Called when the render context is restored on the primary window.
     * @param context The restored render context
     */
    public function onRenderContextRestored(context:RenderContext):Void {}
    
    // --- Text Events ---
    
    /**
     * Called when text is being edited (IME composition).
     * @param text The current composition text
     * @param start The start index of the composition
     * @param length The length of the composition
     */
    public function onTextEdit(text:String, start:Int, length:Int):Void {}
    
    /**
     * Called when text is input.
     * @param text The input text
     */
    public function onTextInput(text:String):Void {}
    
    // --- Touch Events ---
    
    /**
     * Called when a touch is cancelled.
     * @param touch The touch object
     */
    public function onTouchCancel(touch:Touch):Void {}
    
    /**
     * Called when a touch ends.
     * @param touch The touch object
     */
    public function onTouchEnd(touch:Touch):Void {}
    
    /**
     * Called when a touch moves.
     * @param touch The touch object
     */
    public function onTouchMove(touch:Touch):Void {}
    
    /**
     * Called when a touch starts.
     * @param touch The touch object
     */
    public function onTouchStart(touch:Touch):Void {}
    
    // --- Window Events ---
    
    /**
     * Called when the primary window is activated.
     */
    public function onWindowActivate():Void {}
    
    /**
     * Called when the primary window is requested to close.
     */
    public function onWindowClose():Void {}
    
    /**
     * Called when the primary window is created.
     */
    public function onWindowCreate():Void {}
    
    /**
     * Called when the primary window is deactivated.
     */
    public function onWindowDeactivate():Void {}
    
    /**
     * Called when a file is dropped on the primary window.
     * @param file The path of the dropped file
     */
    public function onWindowDropFile(file:String):Void {}
    
    /**
     * Called when the mouse enters the primary window.
     */
    public function onWindowEnter():Void {}
    
    /**
     * Called when the primary window needs to be redrawn.
     */
    public function onWindowExpose():Void {}
    
    /**
     * Called when the primary window gains focus.
     */
    public function onWindowFocusIn():Void {}
    
    /**
     * Called when the primary window loses focus.
     */
    public function onWindowFocusOut():Void {}
    
    /**
     * Called when the primary window enters fullscreen mode.
     */
    public function onWindowFullscreen():Void {}
    
    /**
     * Called when the mouse leaves the primary window.
     */
    public function onWindowLeave():Void {}
    
    /**
     * Called when the primary window is minimized.
     */
    public function onWindowMinimize():Void {}
    
    /**
     * Called when the primary window is moved.
     * @param x The new X position in desktop coordinates
     * @param y The new Y position in desktop coordinates
     */
    public function onWindowMove(x:Float, y:Float):Void {}
    
    /**
     * Called when the primary window is resized.
     * @param width The new width in pixels
     * @param height The new height in pixels
     */
    public function onWindowResize(width:Int, height:Int):Void {}
    
    /**
     * Called when the primary window is restored from minimized or fullscreen.
     */
    public function onWindowRestore():Void {}
    
    // --- Main Loop Events ---
    
    /**
     * Called when rendering should occur on the primary window.
     * @param context The render context to use
     */
    public function render(context:RenderContext):Void {}
    
    /**
     * Called each frame before rendering.
     * @param deltaTime Time in milliseconds since the last update
     */
    public function update(deltaTime:Int):Void {}
    
    // ============================================================================
    // Private Methods
    // ============================================================================
    
    @:noCompletion private function __setupPreloaderEvents():Void 
    {
        __preloader.onProgress.add(onPreloadProgress);
        __preloader.onComplete.add(onPreloadComplete);
    }
    
    @:noCompletion private function __addWindow(window:Window):Void 
    {
        if (window == null) return;
        
        __windows.push(window);
        __windowById.set(window.id, window);
        
        window.onClose.add(__onWindowClose.bind(window), false, -10000);
        
        if (__primaryWindow == null) 
        {
            __primaryWindow = window;
            __connectPrimaryWindowEvents(window);
            onWindowCreate();
        }
        
        onCreateWindow.dispatch(window);
    }
    
    @:noCompletion private function __connectPrimaryWindowEvents(window:Window):Void 
    {
        window.onActivate.add(onWindowActivate);
        window.onDeactivate.add(onWindowDeactivate);
        window.onDropFile.add(onWindowDropFile);
        window.onEnter.add(onWindowEnter);
        window.onExpose.add(onWindowExpose);
        window.onFocusIn.add(onWindowFocusIn);
        window.onFocusOut.add(onWindowFocusOut);
        window.onFullscreen.add(onWindowFullscreen);
        window.onKeyDown.add(onKeyDown);
        window.onKeyUp.add(onKeyUp);
        window.onLeave.add(onWindowLeave);
        window.onMinimize.add(onWindowMinimize);
        window.onMouseDown.add(onMouseDown);
        window.onMouseMove.add(onMouseMove);
        window.onMouseMoveRelative.add(onMouseMoveRelative);
        window.onMouseUp.add(onMouseUp);
        window.onMouseWheel.add(onMouseWheel);
        window.onMove.add(onWindowMove);
        window.onRender.add(render);
        window.onResize.add(onWindowResize);
        window.onRestore.add(onWindowRestore);
        window.onTextEdit.add(onTextEdit);
        window.onTextInput.add(onTextInput);
        window.onRenderContextLost.add(onRenderContextLost);
        window.onRenderContextRestored.add(onRenderContextRestored);
    }
    
    @:noCompletion private function __createWindow(attributes:WindowAttributes):Null<Window> 
    {
        var window = new Window(this, attributes);
        return (window.id == -1) ? null : window;
    }
    
    @:noCompletion private override function __registerLimeModule(application:Application):Void 
    {
        application.onUpdate.add(update);
        application.onExit.add(onModuleExit, false, 0);
        application.onExit.add(__onModuleExit, false, -1000);
        
        for (gamepad in Gamepad.devices) 
        {
            __onGamepadConnect(gamepad);
        }
        Gamepad.onConnect.add(__onGamepadConnect);
        
        for (joystick in Joystick.devices) 
        {
            __onJoystickConnect(joystick);
        }
        Joystick.onConnect.add(__onJoystickConnect);
        
        Touch.onCancel.add(onTouchCancel);
        Touch.onStart.add(onTouchStart);
        Touch.onMove.add(onTouchMove);
        Touch.onEnd.add(onTouchEnd);
    }
    
    @:noCompletion private function __removeWindow(window:Window):Void 
    {
        if (window == null || !__windowById.exists(window.id)) return;
        
        if (__primaryWindow == window) 
        {
            __primaryWindow = null;
        }
        
        __windows.remove(window);
        __windowById.remove(window.id);
        window.close();
        
        __checkForAllWindowsClosed();
    }
    
    @:noCompletion private function __checkForAllWindowsClosed():Void 
    {
        #if !air
        if (__windows.length == 0 && !lime_doc_gen) 
        {
            System.exit(0);
        }
        #end
    }
    
    @:noCompletion private function __onGamepadConnect(gamepad:Gamepad):Void 
    {
        onGamepadConnect(gamepad);
        
        gamepad.onAxisMove.add(onGamepadAxisMove.bind(gamepad));
        gamepad.onButtonDown.add(onGamepadButtonDown.bind(gamepad));
        gamepad.onButtonUp.add(onGamepadButtonUp.bind(gamepad));
        gamepad.onDisconnect.add(onGamepadDisconnect.bind(gamepad));
    }
    
    @:noCompletion private function __onJoystickConnect(joystick:Joystick):Void 
    {
        onJoystickConnect(joystick);
        
        joystick.onAxisMove.add(onJoystickAxisMove.bind(joystick));
        joystick.onButtonDown.add(onJoystickButtonDown.bind(joystick));
        joystick.onButtonUp.add(onJoystickButtonUp.bind(joystick));
        joystick.onDisconnect.add(onJoystickDisconnect.bind(joystick));
        joystick.onHatMove.add(onJoystickHatMove.bind(joystick));
        joystick.onTrackballMove.add(onJoystickTrackballMove.bind(joystick));
    }
    
    @:noCompletion private function __onModuleExit(code:Int):Void 
    {
        if (onExit.canceled) return;
        
        __unregisterLimeModule(this);
        __backend.exit();
        
        if (Application.current == this) 
        {
            Application.current = null;
        }
    }
    
    @:noCompletion private function __onWindowClose(window:Window):Void 
    {
        if (__primaryWindow == window) 
        {
            onWindowClose();
        }
        
        __removeWindow(window);
    }
    
    @:noCompletion private override function __unregisterLimeModule(application:Application):Void 
    {
        application.onUpdate.remove(update);
        application.onExit.remove(__onModuleExit);
        application.onExit.remove(onModuleExit);
        
        Gamepad.onConnect.remove(__onGamepadConnect);
        Joystick.onConnect.remove(__onJoystickConnect);
        Touch.onCancel.remove(onTouchCancel);
        Touch.onStart.remove(onTouchStart);
        Touch.onMove.remove(onTouchMove);
        Touch.onEnd.remove(onTouchEnd);
    }
    
    // ============================================================================
    // Property Accessors
    // ============================================================================
    
    @:noCompletion private inline function get_preloader():Preloader 
    {
        return __preloader;
    }
    
    @:noCompletion private inline function get_window():Window 
    {
        return __primaryWindow;
    }
    
    @:noCompletion private inline function get_windows():Array<Window> 
    {
        return __windows.copy(); // Return a copy to prevent external modification
    }
}

// ============================================================================
// Platform-Specific Backend Type Definitions
// ============================================================================

#if air
@:noCompletion private typedef ApplicationBackend = lime._internal.backend.air.AIRApplication;
#elseif flash
@:noCompletion private typedef ApplicationBackend = lime._internal.backend.flash.FlashApplication;
#elseif (js && html5)
@:noCompletion private typedef ApplicationBackend = lime._internal.backend.html5.HTML5Application;
#else
@:noCompletion private typedef ApplicationBackend = lime._internal.backend.native.NativeApplication;
#end