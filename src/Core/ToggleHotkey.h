#pragma once

namespace GT {
	// Windows messages and DirectInput can report the same physical press
	// in different frames. Keep one latch shared by both input sources.
	class ToggleHotkey {
	public:
		void KeyDown(bool repeat = false) {
			if (!repeat && !messageDown_) {
				if (!latched_)
					requested_ = true;
				latched_ = true;
			}
			messageDown_ = true;
		}

		void KeyUp() { messageDown_ = false; }

		bool Update(bool polledDown) {
			bool toggle = requested_;
			requested_ = false;
			if (polledDown && !latched_) {
				toggle = true;
				latched_ = true;
			}
			// Also preserve a short press whose key-up arrived before this frame.
			// It must be consumed before the latch is rearmed.
			if (!polledDown && !messageDown_ && !toggle)
				latched_ = false;
			return toggle;
		}

		void Reset() { latched_ = messageDown_ = requested_ = false; }

	private:
		bool latched_ = false;
		bool messageDown_ = false;
		bool requested_ = false;
	};
}
