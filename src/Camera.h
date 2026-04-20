#pragma once

#include <windows.h>
#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
private:
    XMFLOAT3 pos = { 0.0f, 1.8f, 0.0f };

    float yaw = XM_PIDIV2;
    float pitch = 0.0f;

    float moveSpeed = 4.0f;
    float mouseSensitivity = 0.003f;

    bool firstMouse = true;

public:
    Camera();

    void Update(float deltaTime, HWND hwnd);
    void HandleMouseLook(HWND hwnd);
    void ResetMouse();

    void ClampAngles();

    XMMATRIX GetViewMatrix() const;
    XMFLOAT3 GetPosition() const;

    XMVECTOR GetForward() const;
    XMVECTOR GetRight() const;
};