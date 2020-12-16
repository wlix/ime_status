#include "pch.h"
#include "ui_automation_client.hpp"

CUIAutomationClient::CUIAutomationClient()
	: m_focus_changed_event_handler(nullptr) {
	HRESULT hr = S_OK;
	hr = ::CoInitialize(NULL);

	hr = m_sp_ui_automation.CoCreateInstance(__uuidof(CUIAutomation));
	ATLASSERT(m_sp_ui_automation);
}


CUIAutomationClient::~CUIAutomationClient() {
	m_focus_changed_event_handler = nullptr;

	m_sp_ui_automation->RemoveAllEventHandlers();
	m_sp_ui_automation.Release();
	::CoUninitialize();
}

void CUIAutomationClient::AddFocusChangedEventHandler(
	std::function<void(IUIAutomationElement*)> eventHandler) {
	HRESULT hr = S_OK;
	if (m_focus_changed_event_handler) {
		hr = m_sp_ui_automation->RemoveFocusChangedEventHandler(
			m_focus_changed_event_handler);
		ATLASSERT(hr == S_OK);
	}
	CComObject<CUIAutomationFocusChangedEventHandler>::CreateInstance(
		&m_focus_changed_event_handler);
	m_focus_changed_event_handler->SetEventHandler(eventHandler);

	hr = m_sp_ui_automation->AddFocusChangedEventHandler(
		nullptr, m_focus_changed_event_handler);
	ATLASSERT(hr == S_OK);
}
