#pragma once

#include <UIAutomationClient.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <functional>

class CUIAutomationFocusChangedEventHandler :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CUIAutomationFocusChangedEventHandler>,
	public IUIAutomationFocusChangedEventHandler {

public:
	BEGIN_COM_MAP(CUIAutomationFocusChangedEventHandler)
		COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
	END_COM_MAP()

	void SetEventHandler(
		std::function<void(IUIAutomationElement*)> eventHandler) {
		m_event_handler = eventHandler;
	}

	virtual HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(
		__RPC__in_opt IUIAutomationElement* sender) override {
		m_event_handler(sender);
		return S_OK;
	}

private:
	std::function<void(IUIAutomationElement*)> m_event_handler;
};

class CUIAutomationClient {
public:
	CUIAutomationClient();
	~CUIAutomationClient();

	CComPtr<IUIAutomation> GetUIAutomation() const {
		return m_sp_ui_automation;
	}

	void AddFocusChangedEventHandler(
		std::function<void(IUIAutomationElement*)> eventHandler);

private:
	CComPtr<IUIAutomation> m_sp_ui_automation;
	CComObject<CUIAutomationFocusChangedEventHandler>
		*m_focus_changed_event_handler;
};
