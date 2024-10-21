#include <vulkan/vulkan.h>
#include "Instance.h"
#include "Window.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
#include "Image.h"

Device* device;
SwapChain* swapChain;
Renderer* renderer;
Camera* camera;
Model* cube;
Blades* blades;
float planeDim = 15.f;
float halfWidth = planeDim * 0.5f;
float ColliSize = 0.5f;

namespace {
    void resizeCallback(GLFWwindow* window, int width, int height) {
        if (width == 0 || height == 0) return;

        vkDeviceWaitIdle(device->GetVkDevice());
        swapChain->Recreate(width, height);
        renderer->RecreateFrameResources();
    }

    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool middleMouseDown = false;
    double previousX = 0.0;
    double previousY = 0.0;

    void mouseDownCallback(GLFWwindow* window, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                leftMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                leftMouseDown = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                rightMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                rightMouseDown = false;
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                middleMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                middleMouseDown = false;
            }
        }
    }

    void mouseMoveCallback(GLFWwindow* window, double xPosition, double yPosition) {
        if (leftMouseDown) {
            double sensitivity = 0.5;
            float deltaX = static_cast<float>((previousX - xPosition) * sensitivity);
            float deltaY = static_cast<float>((previousY - yPosition) * sensitivity);

            camera->UpdateOrbit(deltaX, deltaY, 0.0f);

            previousX = xPosition;
            previousY = yPosition;

        } else if (rightMouseDown) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            float xNDC = (2.0f * xPosition) / width - 1.0f;
            float yNDC = (2.0f * yPosition) / height - 1.0f;

            glm::vec4 rayClip = glm::vec4(xNDC, yNDC, -1.0, 1.0);
            glm::vec4 rayEye = glm::inverse(camera->GetProjectionMatrix()) * rayClip;
            rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0, 0.0);

            glm::vec3 rayWorld = glm::inverse(camera->GetViewMatrix()) * rayEye;
            rayWorld = glm::normalize(rayWorld);

            float t = -camera->GetPosition().y / rayWorld.y;
            glm::vec3 intersectionPoint = camera->GetPosition() + t * rayWorld;

            float clampedX = glm::clamp(intersectionPoint.x, -halfWidth, halfWidth);
            float clampedZ = glm::clamp(intersectionPoint.z, -halfWidth, halfWidth);
            cube->UpdateModelBufferObject(glm::vec4(clampedX, ColliSize, clampedZ , ColliSize));
            blades->UpdateObjectTrans(glm::vec4(clampedX, ColliSize, clampedZ, ColliSize * 2.5));

            previousX = xPosition;
            previousY = yPosition;
        }
        else if (middleMouseDown) {
            
        }
    }

    void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
        if (middleMouseDown) {
            ColliSize += yOffset * 0.1f;
            cube->UpdateModelBufferObject(
                glm::vec4(cube->getModelBufferObject().objectTransform.x,
                    cube->getModelBufferObject().objectTransform.y,
                    cube->getModelBufferObject().objectTransform.z,
                    ColliSize));
            blades->UpdateObjectTrans(
                glm::vec4(blades->GetObjectTransData().objectTrans.x,
                    blades->GetObjectTransData().objectTrans.y,
                    blades->GetObjectTransData().objectTrans.z,
                    ColliSize * 2.5
                )
            );
        }
        else {
            double deltaZ = static_cast<float>(yOffset * 0.5f);
            camera->UpdateOrbit(0.0f, 0.0f, deltaZ);
        }
    }
}

int main() {
    static constexpr char* applicationName = "Vulkan Grass Rendering";
    InitializeWindow(640, 480, applicationName);

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    Instance* instance = new Instance(applicationName, glfwExtensionCount, glfwExtensions);

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance->GetVkInstance(), GetGLFWWindow(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, surface);

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.tessellationShader = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, deviceFeatures);

    swapChain = device->CreateSwapChain(surface, 5);

    camera = new Camera(device, 640.f / 480.f);

    VkCommandPoolCreateInfo transferPoolInfo = {};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.queueFamilyIndex = device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Transfer];
    transferPoolInfo.flags = 0;

    VkCommandPool transferCommandPool;
    if (vkCreateCommandPool(device->GetVkDevice(), &transferPoolInfo, nullptr, &transferCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkImage grassImage;
    VkDeviceMemory grassImageMemory;
    Image::FromFile(device,
        transferCommandPool,
        "images/grass.jpg",
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        grassImage,
        grassImageMemory
    );

    
    Model* plane = new Model(device, transferCommandPool,
        {
            { { -halfWidth, 0.0f, halfWidth }, { 1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } },
            { { halfWidth, 0.0f, halfWidth }, { 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },
            { { halfWidth, 0.0f, -halfWidth }, { 0.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },
            { { -halfWidth, 0.0f, -halfWidth }, { 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }
        },
        { 0, 1, 2, 2, 3, 0 }
    );
    plane->SetTexture(grassImage);

    float halfSize = 1;
    cube = new Model(device, transferCommandPool,
        {
            { {-halfSize, -halfSize, halfSize}, { 1.0f, 1.0f, 1.0f }, { 0.7f, 0.7f } },
            { {halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            { {halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            { {-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            { {-halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            { {halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            { {halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } },
            {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, { 0.7f, 0.7f } }

        },
        { 0, 1, 2, 2, 3, 0,
          4, 6, 5, 7, 6, 4,
          4, 3, 7, 3, 4, 0,
          1, 5, 6, 6, 2, 1,
          3, 2, 6, 6, 7, 3,
          4, 1, 0, 1, 4, 5 }
    );
    cube->SetTexture(grassImage);
    
    blades = new Blades(device, transferCommandPool, planeDim);

    vkDestroyCommandPool(device->GetVkDevice(), transferCommandPool, nullptr);

    Scene* scene = new Scene(device);
    scene->AddModel(plane);
    scene->AddModel(cube);
    scene->AddBlades(blades);

    renderer = new Renderer(device, swapChain, scene, camera);

    glfwSetWindowSizeCallback(GetGLFWWindow(), resizeCallback);
    glfwSetMouseButtonCallback(GetGLFWWindow(), mouseDownCallback);
    glfwSetCursorPosCallback(GetGLFWWindow(), mouseMoveCallback);
    glfwSetScrollCallback(GetGLFWWindow(), scrollCallback);

    while (!ShouldQuit()) {
        glfwPollEvents();
        scene->UpdateTime();
        renderer->Frame();
    }

    vkDeviceWaitIdle(device->GetVkDevice());

    vkDestroyImage(device->GetVkDevice(), grassImage, nullptr);
    vkFreeMemory(device->GetVkDevice(), grassImageMemory, nullptr);

    delete scene;
    delete plane;
    delete cube;
    delete blades;
    delete camera;
    delete renderer;
    delete swapChain;
    delete device;
    delete instance;
    DestroyWindow();
    return 0;
}
