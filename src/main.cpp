#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

constexpr int HIDDEN_OPACITY = 0;
constexpr int PAUSE_LAYER_OPACITY = 75;
constexpr int MAX_OPACITY = 255;
constexpr float BUTTON_SPRITE_SCALE = 0.67f;
constexpr float DEFAULT_BUTTON_SCALE = 1.0f;
constexpr float HIDDEN_BUTTON_SCALE = 50.f; // Scale big enough to cover the entire screen
constexpr const char* NONE_EXCEPTION_ID = "__NONE__";
constexpr const char* SHOW_PERCENTAGE = "0040";
constexpr const char* HIDE_PRACTICE_BUTTONS = "0071";
constexpr const char* SHOW_FPS = "0115";
constexpr const char* SHOW_INFO_LABEL = "0109";
constexpr const char* HIDE_ATTEMPTS = "0134";
constexpr const char* HIDE_PRACTICE_ATTEMPTS = "0135";
constexpr const char* AUDIO_VISUALIZER = "0144";
constexpr const char* SHOW_TIME = "0145";

bool s_autoHideOnPause = false;
bool s_pausedViaKeybind = false;

void setNodeVisible(CCNode* node, bool visible, bool condition) {
	if (node)
		node->setVisible(visible && condition);
}

void changeChildrenVisibility(CCNode* parent, bool visible, std::unordered_set<CCNode*>& nonVisibleNodes, const ZStringView& exceptionID = NONE_EXCEPTION_ID) {
	for (auto* child : parent->getChildrenExt()) {
		if (child->getID() == exceptionID || nonVisibleNodes.contains(child)) continue;

		if (child->isVisible() != visible) {
			child->setVisible(visible);
		} else {
			nonVisibleNodes.insert(child);
		}
	}
}

void handleUILayer(PlayLayer* playLayer, GameManager* gameManager, Mod* mod, bool visible) {
	if (!playLayer->m_uiLayer) return;

	if (mod->getSettingValue<bool>("hide_ui_layer")) {
		playLayer->m_uiLayer->setVisible(visible);
	} else if (gameManager) {
		if (playLayer->m_isPracticeMode && mod->getSettingValue<bool>("hide_practice_buttons"))
			setNodeVisible(playLayer->m_uiLayer->getChildByID("checkpoint-menu"), visible, !gameManager->getGameVariable(HIDE_PRACTICE_BUTTONS));

		if (mod->getSettingValue<bool>("hide_audio_visualizer")) {
			setNodeVisible(playLayer->m_audioVisualizerSFX, visible, gameManager->getGameVariable(AUDIO_VISUALIZER));
			setNodeVisible(playLayer->m_audioVisualizerBG, visible, gameManager->getGameVariable(AUDIO_VISUALIZER));
		}
	}
}

void handleGameManager(PlayLayer* playLayer, GameManager* gameManager, Mod* mod, bool visible) {
	if (!gameManager) return;

	if (playLayer->m_isPlatformer) {
		if (mod->getSettingValue<bool>("hide_time"))
			setNodeVisible(playLayer->m_percentageLabel, visible, gameManager->getGameVariable(SHOW_TIME));
	} else {
		if (mod->getSettingValue<bool>("hide_progress_bar"))
			setNodeVisible(playLayer->m_progressBar, visible, gameManager->m_showProgressBar);

		if (mod->getSettingValue<bool>("hide_percentage"))
			setNodeVisible(playLayer->m_percentageLabel, visible, gameManager->getGameVariable(SHOW_PERCENTAGE));
	}

	if (playLayer->m_isPracticeMode) {
		if (!gameManager->getGameVariable(HIDE_ATTEMPTS) && mod->getSettingValue<bool>("hide_practice_attemps"))
			setNodeVisible(playLayer->m_attemptLabel, visible, !gameManager->getGameVariable(HIDE_PRACTICE_ATTEMPTS));
	} else {
		if (mod->getSettingValue<bool>("hide_attemps"))
			setNodeVisible(playLayer->m_attemptLabel, visible, !gameManager->getGameVariable(HIDE_ATTEMPTS));
	}

	if (mod->getSettingValue<bool>("hide_info_label"))
		setNodeVisible(playLayer->m_infoLabel, visible, gameManager->getGameVariable(SHOW_INFO_LABEL));
}

void handlePlayLayerElements(PlayLayer* playLayer, Mod* mod, bool visible) {
	if (!playLayer) return;

	if (playLayer->m_isPracticeMode && mod->getSettingValue<bool>("hide_checkpoints")) {
		for (auto* checkpoint : CCArrayExt<CheckpointObject>(playLayer->m_checkpointArray)) {
			checkpoint->m_physicalCheckpointObject->setVisible(visible);
		}
	}

	if (mod->getSettingValue<bool>("hide_testmode_label"))
		setNodeVisible(playLayer->getChildByID("testmode-label"), visible, playLayer->m_isTestMode);

	auto* gameManager = GameManager::sharedState();
	handleUILayer(playLayer, gameManager, mod, visible);
	handleGameManager(playLayer, gameManager, mod, visible);
}

void handleOverlayManager(OverlayManager* overlayManager, Mod* mod, bool visible) {
	if (!overlayManager) return;

	if (mod->getSettingValue<bool>("hide_floating_buttons"))
		overlayManager->setVisible(visible);
}

void handleCCDirector(CCDirector* ccDirector, Mod* mod, bool visible) {
	if (!ccDirector) return;

	if (mod->getSettingValue<bool>("hide_fps"))
		ccDirector->m_bDisplayFPS = visible && GameManager::sharedState()->getGameVariable(SHOW_FPS);
}

void applyUserModSettings(Mod* mod, bool visible) {
	handlePlayLayerElements(PlayLayer::get(), mod, visible);
	handleOverlayManager(OverlayManager::get(), mod, visible);
	handleCCDirector(CCDirector::sharedDirector(), mod, visible);
}

void updateHideButton(CCMenuItemSpriteExtra* button, bool visible, std::unordered_set<CCNode*>& nonVisibleNodes) {
	if (!button) return;

	button->stopAllActions();
	button->setOpacity(visible ? MAX_OPACITY : HIDDEN_OPACITY);
	button->setScale(visible ? DEFAULT_BUTTON_SCALE : HIDDEN_BUTTON_SCALE);
}

class $modify(HidePauseMenu, PauseLayer) {
	struct Fields {
		Mod* m_mod = Mod::get();
		CCMenuItemSpriteExtra* m_button = nullptr;
		CCNode* m_background = nullptr;
		std::unordered_set<CCNode*> m_nonVisibleNodes;
	};

	void customSetup() {
		PauseLayer::customSetup();
		if (m_fields->m_mod->getSettingValue<bool>("mod_disabled")) return;

		m_fields->m_background = this->getChildByID("background");
		if (!m_fields->m_background) return;

		if (auto* buttonParent = getButtonParent()) {
			auto* hideBtnSpr = CircleButtonSprite::create(CCSprite::createWithSpriteFrameName("hideBtn_001.png"));
			hideBtnSpr->setScale(BUTTON_SPRITE_SCALE);

			auto* hideBtnButton = CCMenuItemSpriteExtra::create(hideBtnSpr, this, menu_selector(HidePauseMenu::onHideButton));
			hideBtnButton->setID("hide-button"_spr);
			buttonParent->addChild(hideBtnButton);

			buttonParent->updateLayout();
			m_fields->m_button = hideBtnButton;
		}

		// Auto-hide if the keybind toggle is active
		// Skip if pauseAndHide keybind triggered the pause, since it handles hiding itself
		if (s_autoHideOnPause && !s_pausedViaKeybind) {
			Loader::get()->queueInMainThread([this]() {
				onHideButton(this);
			});
		}
	}

	void onResume(CCObject* sender) {
		if (!m_fields->m_background->isVisible())
			onHideButton(sender);

		PauseLayer::onResume(sender);
	}

	void tryQuit(CCObject* sender) {
		if (!m_fields->m_background->isVisible()) {
			s_pausedViaKeybind = false;
			onHideButton(sender);
		} else {
			PauseLayer::tryQuit(sender);
		}
	}

	void onHideButton(CCObject* sender) {
		const bool visible = !m_fields->m_background->isVisible();

		if (m_fields->m_button) {
			changeChildrenVisibility(this, visible, m_fields->m_nonVisibleNodes, m_fields->m_button->getParent()->getID());
			changeChildrenVisibility(m_fields->m_button->getParent(), visible, m_fields->m_nonVisibleNodes, "hide-button"_spr);

			if (m_fields->m_mod->getSettingValue<bool>("hide_hide_button")) {
				changeChildrenVisibility(m_fields->m_button, visible, m_fields->m_nonVisibleNodes);
				updateHideButton(m_fields->m_button, visible, m_fields->m_nonVisibleNodes);
			}
		} else {
			changeChildrenVisibility(this, visible, m_fields->m_nonVisibleNodes);
		}

		PauseLayer::setOpacity(visible ? PAUSE_LAYER_OPACITY : HIDDEN_OPACITY);
		applyUserModSettings(m_fields->m_mod, visible);
		applyModCompatibilityFixes(visible);
	}

	CCNode* getButtonParent() {
		auto buttonPos = m_fields->m_mod->getSettingValue<std::string>("hide_button_pos") + "-button-menu";
		buttonPos[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(buttonPos[0])));
		return buttonPos[0] != 'o' ? this->getChildByID(buttonPos) : nullptr;
	}

	void applyModCompatibilityFixes(bool visible) {
		// Hide "Vanilla Pages" arrows
		if (auto* leftNav = this->getChildByID("left-button-menu-navigation-menu"))
			changeChildrenVisibility(leftNav, visible, m_fields->m_nonVisibleNodes);

		if (auto* rightNav = this->getChildByID("right-button-menu-navigation-menu"))
			changeChildrenVisibility(rightNav, visible, m_fields->m_nonVisibleNodes);

		if (auto* centerNav = this->getChildByID("center-button-menu-navigation-menu"))
			changeChildrenVisibility(centerNav, visible, m_fields->m_nonVisibleNodes);
	}
};

class $modify(EndLevelLayer) {
	struct Fields {
		CCMenuItemSpriteExtra* m_hideButton = nullptr;
		std::unordered_set<CCNode*> m_nonVisibleNodes;
	};

	void customSetup() {
		EndLevelLayer::customSetup();

		this->addEventListener(
			KeybindSettingPressedEventV3(Mod::get(), "hide_pause_menu-keybind"),
			[this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
				if (down && !repeat)
					EndLevelLayer::onHideLayer(this);
			}
		);

		if (auto* hideLayerMenu = getChildByID("hide-layer-menu"))
			m_fields->m_hideButton = typeinfo_cast<CCMenuItemSpriteExtra*>(hideLayerMenu->getChildByID("hide-button"));
	}

	void onHideLayer(CCObject* sender) {
		EndLevelLayer::onHideLayer(sender);
		setLayerVisibility(!EndLevelLayer::m_hidden);
	}

	void onReplay(CCObject* sender) {
		if (EndLevelLayer::m_hidden)
			setLayerVisibility();

		EndLevelLayer::onReplay(sender);
	}

	void onMenu(CCObject* sender) {
		if (EndLevelLayer::m_hidden) {
			onHideLayer(sender);
		} else {
			EndLevelLayer::onMenu(sender);
		}
	}

	void setLayerVisibility(bool visible = true) {
		auto* mod = Mod::get();
		if (mod->getSettingValue<bool>("mod_disabled")) return;

		if (mod->getSettingValue<bool>("end_level_layer_button"))
			applyUserModSettings(mod, visible);

		if (mod->getSettingValue<bool>("hide_end_level_layer_button")) {
			changeChildrenVisibility(m_fields->m_hideButton, visible, m_fields->m_nonVisibleNodes);
			updateHideButton(m_fields->m_hideButton, visible, m_fields->m_nonVisibleNodes);
		}
	}
};

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		s_autoHideOnPause = false;

		this->addEventListener(
			KeybindSettingPressedEventV3(Mod::get(), "hide_pause_menu_toggle-keybind"),
			[this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
				if (down && !repeat)
					toggleAutoHide();
			}
		);

		this->addEventListener(
			KeybindSettingPressedEventV3(Mod::get(), "hide_pause_menu-keybind"),
			[this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
				if (down && !repeat) {
					if (!PlayLayer::m_isPaused) {
						pauseAndHide();
					} else {
						unpause();
					}
				}
			}
		);

		return true;
	}

	inline auto* getPauseLayer() {
		return static_cast<HidePauseMenu*>(typeinfo_cast<PauseLayer*>(CCScene::get()->getChildByID("PauseLayer")));
	}

	void toggleAutoHide() {
		s_autoHideOnPause = !s_autoHideOnPause;
		if (auto* pauseLayer = getPauseLayer())
			if (pauseLayer->m_fields->m_background->isVisible() == s_autoHideOnPause)
				pauseLayer->onHideButton(pauseLayer);
	}

	void pauseAndHide() {
		// Pauses game and hides the pause layer
		s_pausedViaKeybind = true;
		PlayLayer::pauseGame(false);
		if(auto* pauseLayer = getPauseLayer())
			pauseLayer->onHideButton(pauseLayer);
	}

	void unpause() {
		if(auto* pauseLayer = getPauseLayer()) {
			if (s_pausedViaKeybind) {
				// If gameplay have been paused using the keybind, back to gameplay
				pauseLayer->onResume(pauseLayer);
				s_pausedViaKeybind = false;
			} else {
				// If gameplay have been paused not using the keybind, back to pause menu
				pauseLayer->onHideButton(pauseLayer);
			}
		}
	}
};