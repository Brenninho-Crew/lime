package;

import ::APP_MAIN::;
import lime.app.Application;
import lime.ui.WindowAttributes;
import lime.ui.WindowContextAttributes;
import lime.system.System;
import lime.utils.Preloader;
import haxe.ds.StringMap;
import haxe.Exception;
import haxe.CallStack;

#if sys
import sys.FileSystem;
import sys.io.File;
#end

#if neko
import neko.vm.Loader;
import neko.vm.Module;
import haxe.io.Path;
#end

/**
 * Main application entry point for Lime projects.
 * This class handles initialization, window creation, and application lifecycle.
 */
@:access(lime.app.Application)
@:access(lime.system.System)
@:dox(hide)
class ApplicationMain 
{
    // ============================================================================
    // Static Properties
    // ============================================================================
    
    private static var _startTime:Float = 0;
    private static var _initialized:Bool = false;
    private static var _errorHandler:(Exception) -> Void = defaultErrorHandler;
    
    // ============================================================================
    // Public API
    // ============================================================================
    
    /**
     * Main entry point called by Haxe runtime
     */
    public static function main():Void 
    {
        _startTime = haxe.Timer.stamp();
        
        #if (!html5 || munit)
        create(null);
        #end
    }
    
    /**
     * Creates and initializes the application instance
     * @param config Optional configuration object to override default settings
     */
    public static function create(config:Dynamic):Void 
    {
        try 
        {
            // Initialize core systems
            __initializeSystem();
            
            // Load manifest resources if not disabled
            #if !disable_preloader_assets
            __loadManifestResources(config);
            #end
            
            #if !munit
            // Create main application instance
            final app:Application = __createApplication(config);
            
            // Configure application metadata
            __configureApplicationMetadata(app);
            
            // Create main window if needed
            if (app.window == null) 
            {
                __createApplicationWindow(app, config);
            }
            
            // Initialize preloader
            __initializePreloader(app);
            
            // Start the application
            start(app);
            #end
        } 
        catch (exception:Exception) 
        {
            _errorHandler(exception);
        }
    }
    
    /**
     * Starts the application execution
     * @param app The application instance to run
     */
    public static function start(app:Application = null):Void 
    {
        #if !munit
        
        if (app == null) 
        {
            throw new Exception("Application instance is null");
        }
        
        __logPerformance("Application startup complete");
        
        final result:Int = app.exec();
        
        #if (sys && !ios && !nodejs && !webassembly)
        __logPerformance("Application exiting with code: " + result);
        System.exit(result);
        #end
        
        #else
        
        // Unit testing mode - just create instance
        new ::APP_MAIN::();
        
        #end
    }
    
    /**
     * Sets a custom error handler for uncaught exceptions
     * @param handler Function to handle exceptions
     */
    public static function setErrorHandler(handler:(Exception) -> Void):Void 
    {
        _errorHandler = handler != null ? handler : defaultErrorHandler;
    }
    
    // ============================================================================
    // Initialization Methods
    // ============================================================================
    
    /**
     * Initialize core system components
     */
    private static function __initializeSystem():Void 
    {
        if (_initialized) return;
        
        // Register entry point with system
        System.__registerEntryPoint("::APP_FILE::", create);
        
        // Set up global error handling
        #if (sys && !debug)
        haxe.CallStack.exceptionHandler = handleUncaughtException;
        #end
        
        _initialized = true;
        __logDebug("System initialized");
    }
    
    /**
     * Load and initialize manifest resources
     */
    private static function __loadManifestResources(config:Dynamic):Void 
    {
        #if !disable_preloader_assets
        __logDebug("Loading manifest resources...");
        ManifestResources.init(config);
        #end
    }
    
    /**
     * Create the main application instance
     */
    private static function __createApplication(config:Dynamic):Application 
    {
        __logDebug("Creating application instance...");
        return new ::APP_MAIN::();
    }
    
    /**
     * Configure application metadata
     */
    private static function __configureApplicationMetadata(app:Application):Void 
    {
        __logDebug("Configuring application metadata...");
        
        final metadata:StringMap<String> = app.meta;
        
        metadata.set("build", "::meta.buildNumber::");
        metadata.set("company", "::meta.company::");
        metadata.set("file", "::APP_FILE::");
        metadata.set("name", "::meta.title::");
        metadata.set("packageName", "::meta.packageName::");
        metadata.set("version", "::meta.version::");
        metadata.set("startTime", Std.string(_startTime));
        
        #if debug
        metadata.set("buildType", "debug");
        #else
        metadata.set("buildType", "release");
        #end
        
        __logDebug("Application metadata configured");
    }
    
    /**
     * Initialize and configure the preloader
     */
    private static function __initializePreloader(app:Application):Void 
    {
        __logDebug("Initializing preloader...");
        
        #if !disable_preloader_assets
        final preloader:Preloader = app.preloader;
        
        // Add libraries to preloader
        for (library in ManifestResources.preloadLibraries) 
        {
            preloader.addLibrary(library);
            __logDebug('Added preload library: $library');
        }
        
        for (name in ManifestResources.preloadLibraryNames) 
        {
            preloader.addLibraryName(name);
            __logDebug('Added preload library name: $name');
        }
        
        // Set up preloader event handlers
        preloader.onProgress.add(__onPreloadProgress);
        preloader.onComplete.add(__onPreloadComplete);
        preloader.onError.add(__onPreloadError);
        
        // Start loading
        preloader.load();
        #end
    }
    
    // ============================================================================
    // Window Creation
    // ============================================================================
    
    /**
     * Create the main application window
     */
    private static function __createApplicationWindow(app:Application, config:Dynamic):Void 
    {
        __logDebug("Creating application window...");
        
        ::foreach windows::
        final attributes:WindowAttributes = __createWindowAttributes(config);
        __configureWindowContext(attributes);
        __applyConfigOverrides(attributes, config);
        
        #if sys
        __parseCommandLineArguments(attributes);
        #end
        
        app.createWindow(attributes);
        __logDebug('Window created: ::title:: (::width::x::height::)');
        ::end::
        
        #if air
        __configureAirWindow(app);
        #elseif !flash
        __configureLegacyWindow(app);
        #end
    }
    
    /**
     * Create window attributes with default values
     */
    private static function __createWindowAttributes(config:Dynamic):WindowAttributes 
    {
        return {
            allowHighDPI: ::allowHighDPI::,
            alwaysOnTop: ::alwaysOnTop::,
            borderless: ::borderless::,
            display: null,
            element: null,
            frameRate: ::fps::,
            fullscreen: #if web false #else ::fullscreen:: #end,
            height: ::height::,
            hidden: #if munit true #else ::hidden:: #end,
            maximized: ::maximized::,
            minimized: ::minimized::,
            parameters: ::parameters::,
            resizable: ::resizable::,
            title: "::title::",
            width: ::width::,
            x: ::x::,
            y: ::y::
        };
    }
    
    /**
     * Configure window context attributes
     */
    private static function __configureWindowContext(attributes:WindowAttributes):Void 
    {
        attributes.context = {
            antialiasing: ::antialiasing::,
            background: ::background::,
            colorDepth: ::colorDepth::,
            depth: ::depthBuffer::,
            hardware: ::hardware::,
            stencil: ::stencilBuffer::,
            type: null,
            vsync: ::vsync::
        };
    }
    
    /**
     * Apply configuration overrides from config object
     */
    private static function __applyConfigOverrides(attributes:WindowAttributes, config:Dynamic):Void 
    {
        if (config == null) return;
        
        __logDebug("Applying configuration overrides...");
        
        for (field in Reflect.fields(config)) 
        {
            if (Reflect.hasField(attributes, field)) 
            {
                Reflect.setField(attributes, field, Reflect.field(config, field));
                __logDebug('  Override: $field = ${Reflect.field(config, field)}');
            } 
            else if (attributes.context != null && Reflect.hasField(attributes.context, field)) 
            {
                Reflect.setField(attributes.context, field, Reflect.field(config, field));
                __logDebug('  Override context: $field = ${Reflect.field(config, field)}');
            }
        }
    }
    
    /**
     * Parse command line arguments for window settings
     */
    #if sys
    private static function __parseCommandLineArguments(attributes:WindowAttributes):Void 
    {
        System.__parseArguments(attributes);
        __logDebug("Command line arguments parsed");
    }
    #end
    
    /**
     * Configure AIR-specific window settings
     */
    #if air
    private static function __configureAirWindow(app:Application):Void 
    {
        if (app.window != null) 
        {
            app.window.title = "::meta.title::";
            __logDebug('AIR window title set: ::meta.title::');
        }
    }
    #end
    
    /**
     * Configure legacy window settings
     */
    #if (!flash && !air)
    private static function __configureLegacyWindow(app:Application):Void 
    {
        if (app.window != null) 
        {
            app.window.context.attributes.background = ::WIN_BACKGROUND::;
            app.window.frameRate = ::WIN_FPS::;
            __logDebug('Legacy window configured: background=::WIN_BACKGROUND::, fps=::WIN_FPS::');
        }
    }
    #end
    
    // ============================================================================
    // Preloader Event Handlers
    // ============================================================================
    
    private static function __onPreloadProgress(loaded:Int, total:Int):Void 
    {
        final percent:Float = (loaded / total) * 100;
        __logDebug('Preload progress: $loaded/$total (${Math.round(percent)}%)');
    }
    
    private static function __onPreloadComplete():Void 
    {
        __logDebug("Preload complete");
        __logPerformance("Preload finished");
    }
    
    private static function __onPreloadError(message:String):Void 
    {
        __logError('Preload error: $message');
    }
    
    // ============================================================================
    // Error Handling
    // ============================================================================
    
    /**
     * Default error handler
     */
    private static function defaultErrorHandler(exception:Exception):Void 
    {
        __logError('Uncaught exception: ${exception.message}');
        __logError('Stack trace:\n${CallStack.toString(CallStack.exceptionStack())}');
        
        #if sys
        Sys.stderr().writeString('Fatal error: ${exception.message}\n');
        Sys.exit(1);
        #end
    }
    
    /**
     * Handle uncaught exceptions from anywhere in the application
     */
    private static function handleUncaughtException(exception:Dynamic):Void 
    {
        final message:String = Std.string(exception);
        __logError('Uncaught exception: $message');
        __logError('Stack trace:\n${CallStack.toString(CallStack.callStack())}');
        
        // Re-throw for system handler
        throw exception;
    }
    
    // ============================================================================
    // Logging and Performance
    // ============================================================================
    
    private static function __logDebug(message:String):Void 
    {
        #if debug
        Sys.println('[DEBUG] $message');
        #end
    }
    
    private static function __logError(message:String):Void 
    {
        #if sys
        Sys.stderr().writeString('[ERROR] $message\n');
        #else
        trace('[ERROR] $message');
        #end
    }
    
    private static function __logPerformance(message:String):Void 
    {
        final elapsed:Float = haxe.Timer.stamp() - _startTime;
        __logDebug('[PERF] $message (${elapsed * 1000}ms)');
    }
    
    // ============================================================================
    // Static Initialization (Neko)
    // ============================================================================
    
    @:noCompletion
    @:dox(hide)
    public static function __init__():Void 
    {
        // Ensure Application class is initialized
        var init = Application;
        
        #if neko
        __configureNekoPaths();
        #end
    }
    
    #if neko
    /**
     * Configure Neko module paths for proper loading
     */
    private static function __configureNekoPaths():Void 
    {
        __logDebug("Configuring Neko paths...");
        
        final modulePath:String = __getNekoModulePath();
        final loader:Loader = new Loader(untyped $loader);
        
        loader.addPath(Path.directory(modulePath));
        loader.addPath("./");
        loader.addPath("@executable_path/");
        
        __logDebug('Neko paths configured: ${Path.directory(modulePath)}');
    }
    
    /**
     * Get the current Neko module path
     */
    private static function __getNekoModulePath():String 
    {
        final moduleName:String = Module.local().name;
        
        try 
        {
            return FileSystem.fullPath(moduleName);
        } 
        catch (e:Dynamic) 
        {
            // Try with .n extension if not present
            if (!StringTools.endsWith(moduleName, ".n")) 
            {
                try 
                {
                    return FileSystem.fullPath(moduleName + ".n");
                } 
                catch (e2:Dynamic) 
                {
                    // Return original name as fallback
                    return moduleName;
                }
            }
            
            return moduleName;
        }
    }
    #end
}
