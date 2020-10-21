#pragma once

#define SEARCH_PHRASE_SIZE 1024
#define WINDOW_TEXT_SIZE   32767
#define WINDOW_CLASS_SIZE  257

class WindowSearch {
public:
	DWORD mCriteria; // Which criteria are currently in effect (ID, PID, Class, Title, etc.)

	// Controlled and initialized by SetCriteria():
	TCHAR mCriterionTitle[SEARCH_PHRASE_SIZE]; // For storing the title.
	TCHAR mCriterionClass[SEARCH_PHRASE_SIZE]; // For storing the "ahk_class" class name.
	size_t mCriterionTitleLength;             // Length of mCriterionTitle.
	LPTSTR mCriterionExcludeTitle;             // ExcludeTitle.
	size_t mCriterionExcludeTitleLength;      // Length of the above.
	LPTSTR mCriterionText;                     // WinText.
	LPTSTR mCriterionExcludeText;              // ExcludeText.
	HWND mCriterionHwnd;                      // For "ahk_id".
	DWORD mCriterionPID;                      // For "ahk_pid".
	TCHAR mCriterionPath[SEARCH_PHRASE_SIZE]; // For "ahk_exe".

	bool mCriterionPathIsNameOnly;
	bool mFindLastMatch; // Whether to keep searching even after a match is found, so that last one is found.
	int mFoundCount;     // Accumulates how many matches have been found (either 0 or 1 unless mFindLastMatch==true).
	HWND mFoundParent;   // Must be separate from mCandidateParent because some callers don't have access to IsMatch()'s return value.
	HWND mFoundChild;    // Needed by EnumChildFind() to store its result, and other things.

	HWND* mAlreadyVisited;      // Array of HWNDs to exclude from consideration.
	int mAlreadyVisitedCount;   // Count of items in the above.
	int mTimeToWaitForClose;    // Same.
	
	// Controlled by the SetCandidate() method:
	HWND mCandidateParent;
	DWORD mCandidatePID;
	TCHAR mCandidateTitle[WINDOW_TEXT_SIZE];  // For storing title or class name of the given mCandidateParent.
	TCHAR mCandidateClass[WINDOW_CLASS_SIZE]; // Must not share mem with mCandidateTitle because even if ahk_class is in effect, ExcludeTitle can also be in effect.
	TCHAR mCandidatePath[MAX_PATH]; // MAX_PATH vs. T_MAX_PATH because it currently seems to be impossible to run an executable with a longer path (in Windows 10.0.16299).

	void SetCandidate(HWND aWnd) // Must be kept thread-safe since it may be called indirectly by the hook thread.
	{
		// For performance reasons, update the attributes only if the candidate window changed:
		if (mCandidateParent != aWnd)
		{
			mCandidateParent = aWnd;
			UpdateCandidateAttributes(); // In case mCandidateParent isn't NULL, update the PID/Class/etc. based on what was set above.
		}
	}

	void UpdateCandidateAttributes();
	HWND IsMatch(bool aInvert = false);

	WindowSearch() // Constructor.
		: mCriteria(0), mCriterionExcludeTitle(_T(""))
		, mFoundCount(0), mFoundParent(NULL)
		, mFoundChild(NULL)
		, mCandidateParent(NULL)
		, mFindLastMatch(false), mAlreadyVisited(NULL), mAlreadyVisitedCount(0)
	{
	}
};

struct class_and_hwnd_type {
	LPTSTR class_name;
	bool is_found;
	int class_count;
	HWND hwnd;
};