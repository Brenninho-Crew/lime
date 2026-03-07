#include <app/ApplicationEvent.h>
#include <system/CFFI.h>

namespace lime {

    // Static member initialization
    ValuePointer* ApplicationEvent::callback = nullptr;
    ValuePointer* ApplicationEvent::eventObject = nullptr;
    
    // Cached field IDs for Haxe object access
    static int s_deltaTimeFieldId = 0;
    static int s_typeFieldId = 0;
    static bool s_fieldsInitialized = false;

    /**
     * Constructor - Initializes with default values
     */
    ApplicationEvent::ApplicationEvent() 
        : deltaTime(0)
        , type(EventType::UPDATE) {
    }

    /**
     * Dispatches the application event to the registered callback
     * 
     * @param event The event to dispatch
     */
    void ApplicationEvent::Dispatch(ApplicationEvent* event) {
        // Early return if no callback is registered
        if (!ApplicationEvent::callback) {
            return;
        }

        // Initialize field IDs if needed (CFFI path only)
        if (!s_fieldsInitialized && ApplicationEvent::eventObject && 
            ApplicationEvent::eventObject->IsCFFIValue()) {
            
            s_deltaTimeFieldId = val_id("deltaTime");
            s_typeFieldId = val_id("type");
            s_fieldsInitialized = true;
        }

        // Handle based on event object type
        if (ApplicationEvent::eventObject && ApplicationEvent::eventObject->IsCFFIValue()) {
            // CFFI (Haxe) object path
            value haxeObject = static_cast<value>(ApplicationEvent::eventObject->Get());
            
            // Update fields on the Haxe object
            alloc_field(haxeObject, s_deltaTimeFieldId, alloc_int(event->deltaTime));
            alloc_field(haxeObject, s_typeFieldId, alloc_int(static_cast<int>(event->type)));
        } 
        else if (ApplicationEvent::eventObject) {
            // Native C++ object path
            ApplicationEvent* nativeEvent = static_cast<ApplicationEvent*>(
                ApplicationEvent::eventObject->Get()
            );
            
            if (nativeEvent) {
                nativeEvent->deltaTime = event->deltaTime;
                nativeEvent->type = event->type;
            }
        }

        // Call the registered callback
        ApplicationEvent::callback->Call();
    }

}