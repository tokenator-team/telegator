/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_panel.h"

#include "core/file_utilities.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_widget.h"
#include "main/main_session.h"
#include "settings.h"
#include "telegator/telegator_config.h"
#include "telegator/telegator_field.h"
#include "telegator/telegator_journal.h"
#include "telegator/telegator_panel_bridge.h"
#include "telegator/telegator_panel_state.h"
#include "telegator/telegator_requisites.h"
#include "ui/effects/ripple_animation.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "webview/webview_embed.h"
#include "webview/webview_interface.h"
#include "window/window_session_controller.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace Telegator {
namespace {

constexpr auto kPanelWidth = 360;
constexpr auto kPanelMinWidth = 240;
constexpr auto kResizeArea = 6;

[[nodiscard]] rpl::variable<bool> &Shown(
		not_null<Window::SessionController*> controller) {
	static auto map = base::flat_map<
		not_null<Window::SessionController*>,
		std::unique_ptr<rpl::variable<bool>>>();
	auto i = map.find(controller);
	if (i == end(map)) {
		i = map.emplace(
			controller,
			std::make_unique<rpl::variable<bool>>(false)).first;
		controller->lifetime().add([=] {
			map.remove(controller);
		});
	}
	return *i->second;
}

[[nodiscard]] auto Panels()
-> base::flat_map<
		not_null<Window::SessionController*>,
		not_null<SidePanel*>> & {
	static auto result = base::flat_map<
		not_null<Window::SessionController*>,
		not_null<SidePanel*>>();
	return result;
}

[[nodiscard]] Webview::StorageId PanelStorageId() {
	const auto path = cWorkingDir() + u"tdata/telegator-webview"_q;
	auto file = QFile(path + u".token"_q);
	auto token = QByteArray();
	if (file.open(QIODevice::ReadOnly)) {
		token = file.readAll().trimmed();
		file.close();
	}
	if (token.isEmpty()) {
		token = QByteArray::fromStdString(Webview::GenerateStorageToken());
		if (file.open(QIODevice::WriteOnly)) {
			file.write(token);
		}
	}
	return { .path = path, .token = token };
}

// Self-destructing content never leaves the app.
[[nodiscard]] bool MenuAllowed(not_null<HistoryItem*> item) {
	const auto media = item->media();
	return !item->isService()
		&& !item->isEphemeral()
		&& !(media && media->ttlSeconds())
		&& !item->originalText().text.isEmpty()
		&& !item->history()->isForum();
}

void ChooseMenu(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId,
		const QString &id) {
	const auto i = Panels().find(controller);
	const auto item = controller->session().data().message(itemId);
	if (i == end(Panels()) || !item || !MenuAllowed(item)) {
		return;
	}
	const auto peer = item->history()->peer;
	if (peer.get() != controller->activeChatCurrent().peer()) {
		controller->showToast(u"Не выполнено: открыт другой чат."_q);
		return;
	}
	auto details = QJsonObject();
	details.insert(u"item"_q, id);
	JournalWrite(
		&controller->session(),
		u"panel_menu"_q,
		peer,
		item->id,
		item->originalText().text,
		details);
	auto event = QJsonObject();
	event.insert(u"item"_q, id);
	event.insert(u"chat"_q, BridgeChat(&controller->session(), peer));
	event.insert(u"message"_q, BridgeMessage(item));
	i->second->chooseMenu(
		QJsonDocument(event).toJson(QJsonDocument::Compact));
}

// Looks like the other buttons of its place.
class PanelButton final : public Ui::RippleButton {
public:
	PanelButton(
		QWidget *parent,
		const style::IconButton &st,
		const style::color &fg,
		const style::color &fgOver)
	: RippleButton(parent, st.ripple)
	, _st(st)
	, _fg(fg)
	, _fgOver(fgOver) {
		resize(st.width, st.height);
		setCursor(style::cur_pointer);
	}

	void setActive(bool active) {
		_active = active;
		update();
	}

private:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		paintRipple(p, _st.rippleAreaPosition);
		const auto &icon = st::menuIconManage;
		const auto position = (_st.iconPosition.x() < 0)
			? QPoint(
				(width() - icon.width()) / 2,
				(height() - icon.height()) / 2)
			: (_st.iconPosition + QPoint(
				(_st.icon.width() - icon.width()) / 2,
				(_st.icon.height() - icon.height()) / 2));
		const auto &color = _active
			? st::windowActiveTextFg
			: isOver()
			? _fgOver
			: _fg;
		icon.paint(p, position, width(), color->c);
	}

	void onStateChanged(State was, StateChangeSource source) override {
		RippleButton::onStateChanged(was, source);
		update();
	}

	QImage prepareRippleMask() const override {
		const auto size = _st.rippleAreaSize;
		return Ui::RippleAnimation::EllipseMask(QSize(size, size));
	}

	QPoint prepareRippleStartPosition() const override {
		return mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
	}

	const style::IconButton &_st;
	const style::color &_fg;
	const style::color &_fgOver;
	bool _active = false;

};

} // namespace

object_ptr<Ui::RippleButton> MakePanelButton(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		PanelButtonPlace place) {
	if (!PanelAllowed(&controller->session())) {
		return { nullptr };
	}
	const auto compose = (place == PanelButtonPlace::Compose);
	auto result = object_ptr<PanelButton>(
		parent,
		compose ? st::historyAttach : st::topBarInfo,
		compose ? st::historyComposeIconFg : st::menuIconFg,
		compose ? st::historyComposeIconFgOver : st::menuIconFgOver);
	const auto raw = result.data();
	raw->setClickedCallback([=] {
		auto &shown = Shown(controller);
		shown = !shown.current();
	});
	Shown(controller).value() | rpl::on_next([=](bool shown) {
		raw->setActive(shown);
	}, raw->lifetime());
	return result;
}

bool InsertIntoChat(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		TextWithTags text) {
	const auto i = Panels().find(controller);
	if (i == end(Panels())) {
		return false;
	}
	const auto history = i->second->history();
	const auto field = history->telegatorField();
	// History widget is hidden while another section (a topic) is shown.
	if (history->isHidden()
		|| history->peer() != peer.get()
		|| field->isHidden()) {
		return false;
	}
	InsertAtCursor(field, std::move(text));
	field->setFocus();
	return true;
}

void AddMessageActions(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		HistoryItem *item) {
	AddRequisitesAction(menu, controller, item);
	const auto session = &controller->session();
	if (!item
		|| !MenuAllowed(item)
		|| !PanelAllowed(session)
		|| Panel().url.isEmpty()
		|| !Panels().contains(controller)) {
		return;
	}
	const auto itemId = item->fullId();
	for (const auto &entry : SavedMenu(session)) {
		menu->addAction(entry.label, [=, id = entry.id] {
			ChooseMenu(controller, itemId, id);
		}, &st::menuIconManage);
	}
}

PanelToggle::PanelToggle(
	not_null<QWidget*> parent,
	not_null<Window::SessionController*> controller)
: _button(MakePanelButton(parent, controller, PanelButtonPlace::TopBar)) {
}

PanelToggle::~PanelToggle() = default;

void PanelToggle::setAvailable(bool available) {
	if (_button) {
		_button->setVisible(available);
	}
}

int PanelToggle::moveToRight(int right, int top) {
	if (!_button || _button->isHidden()) {
		return right;
	}
	_button->moveToRight(right, top);
	return right + _button->width();
}

SidePanel::SidePanel(
	not_null<QWidget*> parent,
	not_null<Window::SessionController*> controller,
	not_null<HistoryWidget*> history,
	Fn<void()> relayout)
: _controller(controller)
, _history(history)
, _relayout(std::move(relayout))
, _allowed(PanelAllowed(&controller->session())) {
	if (!_allowed) {
		return;
	}
	Panels().remove(controller);
	Panels().emplace(controller, this);
	_width = SavedPanelWidth();
	if (_width <= 0) {
		_width = style::ConvertScale(kPanelWidth);
	}

	_body = base::make_unique_q<Ui::RpWidget>(parent);
	_body->hide();
	_body->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(_body.get());
		p.fillRect(_body->rect(), st::windowBg);
		p.fillRect(0, 0, st::lineWidth, _body->height(), st::shadowFg);
	}, _body->lifetime());
	setupResize();

	Shown(controller).changes() | rpl::on_next([=](bool shown) {
		if (shown) {
			// The panel takes the place of the profile column.
			controller->closeThirdSection();
			if (!std::exchange(_created, true)) {
				createWebview();
			}
		}
		SavePanelShown(shown);
		_relayout();
	}, _body->lifetime());

	// Shown as it was left: the main widget is still being constructed.
	if (SavedPanelShown()) {
		crl::on_main(_body.get(), [=] {
			Shown(controller) = true;
		});
	}

	controller->activeChatValue(
	) | rpl::on_next([=] {
		sendChat();
	}, _body->lifetime());

	style::PaletteChanged(
	) | rpl::on_next([=] {
		sendTheme();
	}, _body->lifetime());
}

SidePanel::~SidePanel() {
	const auto i = Panels().find(_controller);
	if (i != end(Panels()) && i->second.get() == this) {
		Panels().erase(i);
	}
}

not_null<HistoryWidget*> SidePanel::history() const {
	return _history;
}

void SidePanel::chooseMenu(QByteArray event) {
	_menuEvent = std::move(event);
	show();
	sendMenuEvent();
}

void SidePanel::show() {
	// The profile column hides the panel without changing Shown.
	_controller->closeThirdSection();
	Shown(_controller) = true;
	_relayout();
}

int SidePanel::layout(int left, int top, int right, int bottom) {
	if (!_allowed) {
		return 0;
	}
	const auto area = QRect(left, top, right - left, bottom - top);
	_maxWidth = area.width() - st::columnMinimalWidthMain;
	// fitDialogs() makes room; in a very narrow window the chat gets
	// narrower than usual rather than the panel disappearing.
	const auto width = std::min(
		std::max(
			std::min(_width, _maxWidth),
			style::ConvertScale(kPanelMinWidth)),
		area.width() - st::columnMinimalWidthMain / 2);
	if (!Shown(_controller).current() || width <= 0) {
		_body->hide();
		return 0;
	}
	_body->setGeometry(
		area.x() + area.width() - width,
		area.y(),
		width,
		area.height());
	if (_webview && _webview->widget()) {
		// The native page must not cover the resize area.
		const auto skip = style::ConvertScale(kResizeArea);
		_webview->widget()->setGeometry(
			skip,
			0,
			width - skip,
			area.height());
	}
	_body->show();
	return width;
}

int SidePanel::fitDialogs(int dialogsWidth, int bodyWidth) const {
	if (!_allowed || !Shown(_controller).current()) {
		return dialogsWidth;
	}
	const auto min = style::ConvertScale(kPanelMinWidth);
	const auto chat = st::columnMinimalWidthMain;
	if (bodyWidth - dialogsWidth - chat >= min) {
		return dialogsWidth;
	}
	// The full chats list while the panel fits at its own width, then at
	// its minimal width, else the list of photos only, as Telegram does.
	const auto full = bodyWidth - chat - std::max(_width, min);
	if (full >= st::columnMinimalWidthLeft) {
		return std::min(dialogsWidth, full);
	} else if (bodyWidth - chat - min >= st::columnMinimalWidthLeft) {
		return std::min(dialogsWidth, int(st::columnMinimalWidthLeft));
	}
	return std::min(dialogsWidth, _controller->dialogsSmallColumnWidth());
}

void SidePanel::setupResize() {
	const auto handle = Ui::CreateChild<Ui::RpWidget>(_body.get());
	handle->setCursor(style::cur_sizehor);
	_body->sizeValue() | rpl::on_next([=](QSize size) {
		handle->setGeometry(
			0,
			0,
			style::ConvertScale(kResizeArea),
			size.height());
	}, handle->lifetime());

	struct Drag {
		std::optional<int> fromX;
		int fromWidth = 0;
	};
	const auto drag = handle->lifetime().make_state<Drag>();
	handle->events() | rpl::on_next([=](not_null<QEvent*> e) {
		const auto type = e->type();
		if (type != QEvent::MouseButtonPress
			&& type != QEvent::MouseMove
			&& type != QEvent::MouseButtonRelease) {
			return;
		}
		const auto x = static_cast<QMouseEvent*>(
			e.get())->globalPosition().toPoint().x();
		if (type == QEvent::MouseButtonPress) {
			drag->fromX = x;
			drag->fromWidth = _body->width();
		} else if (!drag->fromX) {
			return;
		} else if (type == QEvent::MouseMove) {
			const auto min = style::ConvertScale(kPanelMinWidth);
			_width = std::clamp(
				drag->fromWidth + (*drag->fromX - x),
				min,
				std::max(min, _maxWidth));
			_relayout();
		} else {
			drag->fromX = std::nullopt;
			SavePanelWidth(_width);
		}
	}, handle->lifetime());
	handle->show();
}

void SidePanel::createWebview() {
	const auto url = Panel().url;
	if (url.isEmpty()) {
		showNotice(u"Панель не настроена."_q);
		return;
	}
	_webview = std::make_unique<Webview::Window>(
		_body.get(),
		Webview::WindowConfig{
			.opaqueBg = st::windowBg->c,
			.storageId = PanelStorageId(),
			.safe = true,
		});
	const auto raw = _webview.get();
	if (!raw->widget()) {
		LOG(("Telegator: panel webview is not available."));
		_webview = nullptr;
		showNotice(u"Встроенный браузер недоступен."_q);
		return;
	}
	raw->widget()->show();
	raw->setNavigationStartHandler([=](const QString &uri, bool newWindow) {
		if (newWindow) {
			File::OpenUrl(uri);
			return false;
		}
		return allowedNavigation(uri);
	});
	raw->setNavigationDoneHandler([=](bool success) {
		LOG(("Telegator: panel page %1."
			).arg(success ? "loaded" : "failed to load"));
		if (success) {
			pageReady();
		}
	});
	raw->setMessageHandler([=](const QJsonDocument &message) {
		crl::on_main(_body.get(), [=] {
			handleMessage(message);
		});
	});
	raw->init(BridgeScript());
	raw->navigate(url);
}

void SidePanel::showNotice(const QString &text) {
	const auto label = Ui::CreateChild<Ui::FlatLabel>(_body.get(), text);
	_body->sizeValue() | rpl::on_next([=](QSize size) {
		const auto skip = style::ConvertScale(16);
		label->resizeToWidth(size.width() - 2 * skip);
		label->moveToLeft(skip, skip);
	}, label->lifetime());
	label->show();
}

bool SidePanel::allowedNavigation(const QString &uri) const {
	const auto target = QUrl(uri);
	const auto base = QUrl(Panel().url);
	return (target.scheme() == base.scheme())
		&& (target.host() == base.host())
		&& (target.port() == base.port());
}

void SidePanel::handleMessage(const QJsonDocument &message) {
	const auto object = message.object();
	const auto event = object.value(u"event"_q).toString();
	if (event == u"ready"_q) {
		pageReady();
	} else if (event == u"set_menu"_q) {
		SaveMenu(
			&_controller->session(),
			ParseMenu(object.value(u"items"_q).toArray()));
	} else if (event == u"open"_q) {
		show();
	} else if (event == u"insert_text"_q) {
		// The chat the page meant must still be the open one.
		const auto chat = object.value(u"chat"_q).toString();
		const auto peer = _controller->activeChatCurrent().peer();
		if (!peer) {
			return;
		} else if (chat != QString::number(peer->id.value)) {
			_controller->showToast(u"Не выполнено: открыт другой чат."_q);
			return;
		}
		const auto text = object.value(u"text"_q).toString();
		if (!text.isEmpty()) {
			JournalWrite(
				&_controller->session(),
				u"panel_insert"_q,
				peer,
				0,
				text);
			InsertIntoChat(_controller, peer, { text, {} });
		}
	} else if (event == u"operator"_q) {
		JournalSetOperator(
			&_controller->session(),
			object.value(u"name"_q).toString());
	}
}

void SidePanel::pageReady() {
	_pageReady = true;
	sendTheme();
	sendChat();
	sendMenuEvent();
}

void SidePanel::eval(const QByteArray &script) {
	if (_webview && _pageReady) {
		_webview->eval(script);
	}
}

void SidePanel::sendChat() {
	const auto json = QJsonDocument(BridgeChat(
		&_controller->session(),
		_controller->activeChatCurrent().peer())).toJson(
			QJsonDocument::Compact);
	eval("window.Telegator && window.Telegator._set("
		"'chat', 'onchat', " + json + ");");
}

void SidePanel::sendTheme() {
	const auto json = QJsonDocument(BridgeTheme()).toJson(
		QJsonDocument::Compact);
	eval("window.Telegator && window.Telegator._set("
		"'theme', 'ontheme', " + json + ");");
}

void SidePanel::sendMenuEvent() {
	if (_menuEvent && _webview && _pageReady) {
		eval("window.Telegator && window.Telegator._fire('onmenu', "
			+ *base::take(_menuEvent)
			+ ");");
	}
}

} // namespace Telegator
