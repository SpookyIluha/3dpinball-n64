#pragma once

class n64_input
{
public:
    static void Initialize();
    static void ScanPads();
    static void Clear();

    static bool Exit();
    static bool Pause();
    static bool NewGame();
    static bool ToggleMusic();

    static bool LaunchBallDown();
    static bool LaunchBallUp();

    static bool MoveLeftPaddleDown();
    static bool MoveLeftPaddleUp();
    static bool MoveRightPaddleDown();
    static bool MoveRightPaddleUp();

    static bool NudgeLeftDown();
    static bool NudgeLeftUp();
    static bool NudgeRightDown();
    static bool NudgeRightUp();
    static bool NudgeUpDown();
    static bool NudgeUpUp();

    static bool Button1();
    static bool Button2();

private:
    static unsigned int n64ButtonsDown;
    static unsigned int n64ButtonsUp;
    static unsigned int n64ButtonsHeld;
    static bool n64LeftTriggerDown;
    static bool n64LeftTriggerUp;
    static bool n64RightTriggerDown;
    static bool n64RightTriggerUp;
};
