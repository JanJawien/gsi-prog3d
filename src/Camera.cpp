#include "Camera.h"
#include <algorithm>

Camera::Camera()
{
}

void Camera::Update(float deltaTime, HWND hwnd)
{
    if (GetForegroundWindow() != hwnd)
    {
        firstMouse = true;
        return;
    }

    XMVECTOR forward = GetForward();
    XMVECTOR right = GetRight();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR p = XMLoadFloat3(&pos);

    if (GetAsyncKeyState('W') & 0x8000) p = XMVectorAdd(p, XMVectorScale(forward, moveSpeed * deltaTime));
    if (GetAsyncKeyState('S') & 0x8000) p = XMVectorSubtract(p, XMVectorScale(forward, moveSpeed * deltaTime));
    if (GetAsyncKeyState('D') & 0x8000) p = XMVectorAdd(p, XMVectorScale(right, moveSpeed * deltaTime));
    if (GetAsyncKeyState('A') & 0x8000) p = XMVectorSubtract(p, XMVectorScale(right, moveSpeed * deltaTime));
    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) p = XMVectorAdd(p, XMVectorScale(up, moveSpeed * deltaTime));
    if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) p = XMVectorSubtract(p, XMVectorScale(up, moveSpeed * deltaTime));

    XMStoreFloat3(&pos, p);

    HandleMouseLook(hwnd);
}

void Camera::HandleMouseLook(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    POINT center;
    center.x = (rect.right - rect.left) / 2;
    center.y = (rect.bottom - rect.top) / 2;

    POINT screenCenter = center;
    ClientToScreen(hwnd, &screenCenter);

    POINT mousePos;
    GetCursorPos(&mousePos);

    if (firstMouse)
    {
        SetCursorPos(screenCenter.x, screenCenter.y);
        firstMouse = false;
        return;
    }

    int dx = mousePos.x - screenCenter.x;
    int dy = mousePos.y - screenCenter.y;

    yaw += dx * mouseSensitivity;
    pitch -= dy * mouseSensitivity;

    ClampAngles();

    SetCursorPos(screenCenter.x, screenCenter.y);
}

void Camera::ResetMouse()
{
    firstMouse = true;
}

void Camera::ClampAngles()
{
    const float limit = XM_PIDIV2 - 0.1f;

    if (pitch > limit) pitch = limit;
    if (pitch < -limit) pitch = -limit;
}

XMMATRIX Camera::GetViewMatrix() const
{
    XMVECTOR p = XMLoadFloat3(&pos);
    XMVECTOR forward = GetForward();
    XMVECTOR target = XMVectorAdd(p, forward);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    return XMMatrixLookAtLH(p, target, up);
}

XMFLOAT3 Camera::GetPosition() const
{
    return pos;
}

XMVECTOR Camera::GetForward() const
{
    float cp = cosf(pitch);
    float sp = sinf(pitch);
    float cy = cosf(yaw);
    float sy = sinf(yaw);

    return XMVector3Normalize(XMVectorSet(sy * cp, sp, cy * cp, 0.0f));
}

XMVECTOR Camera::GetRight() const
{
    XMVECTOR forward = GetForward();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMVector3Normalize(XMVector3Cross(up, forward));
}