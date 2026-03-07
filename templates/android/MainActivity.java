package ::APP_PACKAGE::;

import android.os.Bundle;
import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

/**
 * Main activity for the Lime application
 * 
 * This activity serves as the entry point for the Android application
 * and extends the base Lime GameActivity which handles the game loop,
 * input events, and rendering.
 */
public class MainActivity extends org.haxe.lime.GameActivity {
    
    /**
     * Called when the activity is first created
     * 
     * @param savedInstanceState If the activity is being re-initialized after 
     *                           previously being shut down, this contains the data
     *                           it most recently supplied in onSaveInstanceState(Bundle)
     */
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Any custom initialization code can be added here
        // For example: analytics, crash reporting, etc.
    }
    
    /**
     * Called when the activity is becoming visible to the user
     */
    @Override
    protected void onStart() {
        super.onStart();
        // Custom onStart behavior
    }
    
    /**
     * Called when the activity will start interacting with the user
     */
    @Override
    protected void onResume() {
        super.onResume();
        // Custom onResume behavior
    }
    
    /**
     * Called when the system is about to start resuming a previous activity
     */
    @Override
    protected void onPause() {
        super.onPause();
        // Custom onPause behavior
    }
    
    /**
     * Called when the activity is no longer visible to the user
     */
    @Override
    protected void onStop() {
        super.onStop();
        // Custom onStop behavior
    }
    
    /**
     * Called when the activity is being destroyed
     */
    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Custom cleanup code
    }
    
    /**
     * Called to save UI state changes before the activity may be killed
     * 
     * @param outState Bundle in which to place your saved state
     */
    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        // Save any custom state here
    }
    
    /**
     * Called when the activity is re-created after being destroyed
     * 
     * @param savedInstanceState The data most recently supplied in onSaveInstanceState
     */
    @Override
    protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        // Restore any custom state here
    }
    
    /**
     * Called when the activity has detected the user's press of the back key
     */
    @Override
    public void onBackPressed() {
        // Custom back button handling if needed
        super.onBackPressed();
    }
    
    /**
     * Called when the activity is started for the first time or after a configuration change
     */
    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        // Window attachment handling
    }
    
    /**
     * Called when the activity is no longer attached to its window
     */
    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Window detachment handling
    }
}
