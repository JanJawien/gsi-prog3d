#pragma once

#include <windows.h>
#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
private:
    // starting camera position in the scene
    XMFLOAT3 pos = { 0.0f, 1.8f, 0.0f };

    // camera rotation left/right and up/down
    float yaw = XM_PIDIV2;
    float pitch = 0.0f;

    // change this to modify camera movement speed
    float moveSpeed = 4.0f;

    // change this to modify mouse sensitivity
    float mouseSensitivity = 0.003f;

    // prevents mouse jump on the first mouse update
    bool firstMouse = true;

public:
    Camera();

    // updates camera movement and mouse look
    void Update(float deltaTime, HWND hwnd);

    // handles camera rotation with mouse
    void HandleMouseLook(HWND hwnd);

    // resets mouse state after window loses focus
    void ResetMouse();

    // limits looking up/down
    void ClampAngles();

    // camera view matrix used for rendering
    XMMATRIX GetViewMatrix() const;

    // camera position, also used for audio and lighting
    XMFLOAT3 GetPosition() const;

    // camera forward direction
    XMVECTOR GetForward() const;

    // camera right direction
    XMVECTOR GetRight() const;
};
