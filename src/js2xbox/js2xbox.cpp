#include "js2xbox.h"

#include "events.h"
#include "uinput.h"
#include "joypads.h"
#include <signal.h>
#include <memory>
#include <string.h>
#include <thread>
#include <future>

#include "../logger.h"
#include "../globals.h"

namespace {
	volatile bool run = true;

	void sigint(int signo) {
		logger.log(Logger::INF, "Exiting... Cleaning up virtual devices...");
		run = false;
		signal(signo, SIG_DFL);
	}
}

joypad initJs2xbox() {
    std::promise<joypad> device_promise;
    std::future<joypad> device_future = device_promise.get_future();

    bool virtualDeviceNeeded = false;  // New flag to track if the device is needed

    // Create a new thread
    std::thread js2xbox_thread([&device_promise, &virtualDeviceNeeded]() {
        try {
            const char* target_pad_name = "oga_joypad";
            events::js_desc const* j_target = nullptr;

            logger.log(Logger::INF, "Checking joypad: %s", target_pad_name);

            // Find the specified target joypad
            for (const events::js_desc** p = joypads::out; *p; ++p) {
                if (!strcasecmp((*p)->i_name, target_pad_name)) {
                    j_target = *p;
                    break;
                }
            }

            if (!j_target) {
                throw std::runtime_error(std::string("Target Joypad '") + target_pad_name + "' is not valid.");
            }

            logger.log(Logger::INF, "Using target virtual Joypad as %s", j_target->i_name);

            // Register the Ctrl-C callback
            if (signal(SIGINT, sigint) == SIG_ERR)
                throw std::runtime_error("Can't override SIGINT");
            if (signal(SIGTERM, sigint) == SIG_ERR)
                throw std::runtime_error("Can't override SIGTERM");

            const events::js_desc** js_list = joypads::in;
            events::js_desc const* j_found = nullptr;

            // Get the joypad object
            joy = events::find_event_js(js_list, &j_found);
            const std::string dev_name = joy.event;

            // Open the handle
            uinput::evt_reader reader(dev_name.c_str());
            std::unique_ptr<uinput::pad> pad(uinput::get_pad(j_found, j_target));

            // **Check if the virtual device is needed**
            if (!pad) {
                logger.log(Logger::INF, "Virtual device not needed...");
                device_promise.set_value(joy); // Send `joypad` object
                return; // **Exit the thread early**
            }

            virtualDeviceNeeded = true;  // Set the flag since the virtual device is needed

            char dev_path[128] = {0};
            input_event ie = {0};

            pad->get_device_name(dev_path, sizeof(dev_path));
            joy.setValues(std::string(dev_path), joy.rumble, true); // Update the `event` field

            device_promise.set_value(joy); // Send updated joypad object

            logger.log(Logger::INF, "Virtual device created at '%s'", dev_path);

            // Keep running in background
            while (run) {
                if (reader.read(ie) && pad->translate_event(ie))
                    pad->send_event(ie);
            }

        } catch (const std::exception& e) {
            logger.log(Logger::ERR, "Exception: %s", e.what());
            device_promise.set_value(joypad()); // Send an empty joypad object on error
        } catch (...) {
            logger.log(Logger::ERR, "Unknown exception");
            device_promise.set_value(joypad()); // Send an empty joypad object on error
        }

        logger.log(Logger::INF, "Virtual device destroyed");
    });

    // **Check if we need to join the thread instead of detaching**
    joypad jp_result = device_future.get();

    if (!virtualDeviceNeeded) { // Use the new flag instead of checking strings
        logger.log(Logger::INF, "No virtual device created, joining thread...");
        js2xbox_thread.join(); // Ensure the thread exits safely
    } else {
        js2xbox_thread.detach(); // Detach only if it's running indefinitely
    }

    return jp_result; // Return the `joypad` object
}