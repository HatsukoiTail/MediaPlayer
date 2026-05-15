#include "native.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

void enableFrameless(QWidget *window)
{
    window->setWindowFlag(Qt::FramelessWindowHint);
#ifdef Q_OS_WIN
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    const LONG style = ( WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN );
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    // 使用 DWM API 扩展玻璃区域，实现“隐藏边框”的视觉效果
    MARGINS shadow = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
#endif
}

bool handleNativeEvent(void *message, qintptr *result)
{
    return true;

#ifndef Q_OS_WIN
    auto msg = static_cast<MSG*>(message);
    switch(msg->message)
    {
    case WM_NCCALCSIZE:
    {
        *result = 0;
        return true;
    }
    case WM_GETMINMAXINFO:
    {
        if (IsZoomed(msg->hwnd))
        {
            RECT frame = {0, 0, 0, 0};
            AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
            frame.left = std::abs(frame.left);
            frame.top = std::abs(frame.bottom);
            this->setContentsMargins(frame.left, frame.top, frame.right, frame.bottom);
            this->layout->setContentsMargins(0, 0, 0, 0);
        }
        else
        {
            this->setContentsMargins(0, 0, 0, 0);
            this->layout->setContentsMargins(0, 0, 0, 0);
            SetWindowPos(msg->hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        }
        *result = DefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        return true;
    }
    case WM_NCHITTEST:
    {
        const LONG borderWidth = 5;
        RECT winrect;
        GetWindowRect(msg->hwnd, &winrect);
        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);

        // bottom left
        if (x >= winrect.left && x < winrect.left + borderWidth &&
            y < winrect.bottom && y >= winrect.bottom - borderWidth)
        {
            *result = HTBOTTOMLEFT;
            return true;
        }

        // bottom right
        if (x < winrect.right && x >= winrect.right - borderWidth &&
            y < winrect.bottom && y >= winrect.bottom - borderWidth)
        {
            *result = HTBOTTOMRIGHT;
            return true;
        }

        // top left
        if (x >= winrect.left && x < winrect.left + borderWidth &&
            y >= winrect.top && y < winrect.top + borderWidth)
        {
            *result = HTTOPLEFT;
            return true;
        }

        // top right
        if (x < winrect.right && x >= winrect.right - borderWidth &&
            y >= winrect.top && y < winrect.top + borderWidth)
        {
            *result = HTTOPRIGHT;
            return true;
        }

        // left
        if (x >= winrect.left && x < winrect.left + borderWidth)
        {
            *result = HTLEFT;
            return true;
        }

        // right
        if (x < winrect.right && x >= winrect.right - borderWidth)
        {
            *result = HTRIGHT;
            return true;
        }

        // bottom
        if (y < winrect.bottom && y >= winrect.bottom - borderWidth)
        {
            *result = HTBOTTOM;
            return true;
        }

        // top
        if (y >= winrect.top && y < winrect.top + borderWidth)
        {
            *result = HTTOP;
            return true;
        }

        QPoint pos = mapFromGlobal(QCursor::pos());
        if (this->title->rect().contains(pos))
        {
            const auto buttons = this->title->findChildren<QAbstractButton*>();
            for (const auto* button : buttons)
            {
                if (button->isVisible() && button->geometry().contains(pos - this->title->pos()))
                    return false;
            }
            *result = HTCAPTION;
            return true;
        }

        return false;
    }

#endif
}
