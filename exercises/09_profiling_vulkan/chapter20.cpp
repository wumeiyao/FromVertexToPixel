#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

enum class ResourceState {
    Undefined,
    DepthAttachmentWrite,
    ShaderRead,
    ColorAttachmentWrite,
    Present
};

const char* name(ResourceState state) {
    switch (state) {
        case ResourceState::Undefined: return "Undefined";
        case ResourceState::DepthAttachmentWrite: return "DepthAttachmentWrite";
        case ResourceState::ShaderRead: return "ShaderRead";
        case ResourceState::ColorAttachmentWrite: return "ColorAttachmentWrite";
        case ResourceState::Present: return "Present";
    }
    return "Unknown";
}

struct Resource {
    std::string name;
    ResourceState state = ResourceState::Undefined;
};

class CommandBuffer {
public:
    void begin(int frameSlot, int swapchainImage) {
        if (recording_) throw std::logic_error("Command buffer is already recording");
        recording_ = true;
        commands_.clear();
        commands_.push_back("Begin frame slot " + std::to_string(frameSlot) +
                            ", swapchain image " + std::to_string(swapchainImage));
    }

    void transition(Resource& resource, ResourceState next) {
        requireRecording();
        commands_.push_back("Barrier " + resource.name + ": " + name(resource.state) +
                            " -> " + name(next));
        resource.state = next;
    }

    void pass(const std::string& passName, const std::string& action) {
        requireRecording();
        commands_.push_back(passName + ": " + action);
    }

    void end() {
        requireRecording();
        commands_.push_back("End command buffer");
        recording_ = false;
    }

    const std::vector<std::string>& commands() const { return commands_; }

private:
    void requireRecording() const {
        if (!recording_) throw std::logic_error("Command buffer is not recording");
    }

    bool recording_ = false;
    std::vector<std::string> commands_;
};

struct FrameSlot {
    bool fenceSignaled = true;
};

int main() {
    std::array<FrameSlot, 2> frameSlots{};
    const std::array<int, 3> acquiredImages{1, 0, 2};
    Resource shadowDepth{"shadowDepth"};
    std::array<Resource, 3> swapchainImages{{
        {"swapchain[0]", ResourceState::Present},
        {"swapchain[1]", ResourceState::Present},
        {"swapchain[2]", ResourceState::Present}}};

    for (int frame = 0; frame < 3; ++frame) {
        const int slotIndex = frame % static_cast<int>(frameSlots.size());
        const int imageIndex = acquiredImages[static_cast<std::size_t>(frame)];
        FrameSlot& slot = frameSlots[static_cast<std::size_t>(slotIndex)];
        if (!slot.fenceSignaled) {
            std::cout << "CPU waits for fence of frame slot " << slotIndex << '\n';
            slot.fenceSignaled = true;
        }
        slot.fenceSignaled = false;

        CommandBuffer commands;
        commands.begin(slotIndex, imageIndex);
        commands.transition(shadowDepth, ResourceState::DepthAttachmentWrite);
        commands.pass("Shadow", "clear depth and draw casters");
        commands.transition(shadowDepth, ResourceState::ShaderRead);
        Resource& presentImage = swapchainImages[static_cast<std::size_t>(imageIndex)];
        commands.transition(presentImage, ResourceState::ColorAttachmentWrite);
        commands.pass("Opaque", "sample shadowDepth and write scene color");
        commands.pass("UI", "load color and draw overlay");
        commands.transition(presentImage, ResourceState::Present);
        commands.end();

        std::cout << "\nFrame " << frame << " command list\n";
        for (const std::string& command : commands.commands()) {
            std::cout << "  " << command << '\n';
        }
        std::cout << "Queue submit: image-available semaphore -> GPU work -> "
                     "render-finished semaphore -> present\n";

        // The demo immediately signals the previous slot on the next CPU frame to
        // model asynchronous completion without pretending record == execute.
        frameSlots[static_cast<std::size_t>((slotIndex + 1) % 2)].fenceSignaled = true;
    }

    std::cout << "\nFrame slot sequence: 0,1,0; swapchain image sequence: 1,0,2\n"
              << "They are deliberately different concepts.\n";
    return 0;
}
