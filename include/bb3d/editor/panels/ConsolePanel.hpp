#pragma once

#if defined(BB3D_ENABLE_EDITOR)

#include "bb3d/editor/EditorPanel.hpp"
#include <string>
#include <vector>

namespace bb3d::editor {

/**
 * @brief Log entry stored in the ConsolePanel ring buffer.
 */
struct ConsoleMessage {
    enum class Level { Trace, Info, Warn, Error };
    Level level = Level::Info;
    std::string text;
};

/**
 * @brief Console log viewer panel with level filtering, search, and auto-scroll.
 */
class ConsolePanel : public EditorPanel {
public:
    static constexpr std::string_view kId = "bb3d_console";
    static constexpr std::string_view kTitle = "Console";

    ConsolePanel();
    ~ConsolePanel() override = default;

    void onImGuiRender() override;

    [[nodiscard]] std::string_view getId() const override { return kId; }
    [[nodiscard]] std::string_view getTitle() const override { return kTitle; }

    void addMessage(ConsoleMessage::Level level, std::string_view text);
    void clear();

private:
    std::vector<ConsoleMessage> m_messages;
    bool m_filterTrace = false;
    bool m_filterInfo = true;
    bool m_filterWarn = true;
    bool m_filterError = true;
    bool m_autoScroll = true;
};

} // namespace bb3d::editor

#endif // BB3D_ENABLE_EDITOR
