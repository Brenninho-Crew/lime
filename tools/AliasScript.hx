package;

import haxe.macro.Compiler;
import sys.io.Process;
import sys.FileSystem;

/**
 * Alias script for executing Haxelib commands with proper argument handling
 */
class AliasScript 
{
    /**
     * Main entry point
     */
    public static function main():Void 
    {
        // Get the command to execute from compiler define
        final command:String = getCommand();
        
        // Build arguments list
        final args:Array<String> = buildArguments(command);
        
        // Execute the command and handle result
        final exitCode:Int = executeCommand(args);
        
        // Exit with the same code as the executed command
        Sys.exit(exitCode);
    }
    
    /**
     * Retrieves the command from compiler defines
     * @return The command name
     */
    static function getCommand():String 
    {
        final command:String = Compiler.getDefine("command");
        
        if (command == null || command.length == 0) 
        {
            Sys.stderr().writeString("Error: No command specified. Define 'command' in your build configuration.\n");
            Sys.exit(1);
        }
        
        return command;
    }
    
    /**
     * Builds the argument list for haxelib execution
     * @param command The base command to run
     * @return Array of arguments for haxelib
     */
    static function buildArguments(command:String):Array<String> 
    {
        final args:Array<String> = ["run", command];
        
        // Add any additional arguments passed to this script
        final scriptArgs:Array<String> = Sys.args();
        
        if (scriptArgs != null && scriptArgs.length > 0) 
        {
            args = args.concat(scriptArgs);
        }
        
        return args;
    }
    
    /**
     * Executes the haxelib command with the given arguments
     * @param args The arguments to pass to haxelib
     * @return The exit code of the executed command
     */
    static function executeCommand(args:Array<String>):Int 
    {
        try 
        {
            final exitCode:Int = Sys.command("haxelib", args);
            
            if (exitCode != 0) 
            {
                Sys.stderr().writeString('Warning: haxelib command exited with code $exitCode\n');
            }
            
            return exitCode;
        } 
        catch (error:Dynamic) 
        {
            Sys.stderr().writeString('Error executing haxelib: $error\n');
            return 1;
        }
    }
    
    /**
     * Optional: Debug method to print what would be executed
     */
    #if debug
    static function printDebugInfo(command:String, args:Array<String>):Void 
    {
        Sys.stdout().writeString('Command: haxelib ${args.join(" ")}\n');
    }
    #end
}
