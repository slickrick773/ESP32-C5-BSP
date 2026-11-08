#pragma once
#include "sample_ui/core.h"
#include <array>
namespace sample_ui {
struct ControlModel {const char* label;Action action;Availability state=Availability::Available;};
struct ListRow {
    ItemId id;const char* label;const char* value;Action action;
    Availability state=Availability::Available;
};
struct ListModel {
    const char* title;
    const ListRow* rows;
    std::size_t count, first;
    ItemId selected;
    std::array<ControlModel,2> footer;
    int scrollOffset=-1; // Negative: paged first; otherwise continuous pixels.
};
struct ListLayout {
    Chrome frame{};
    std::array<Rect,9> rows{};
    std::array<Rect,2> footer{};
    std::size_t visible=0,first=0;
    Rect viewport{},rail{},thumb{};
};
ListLayout listLayout(Size size,std::size_t count,std::size_t first);
int listScrollLimit(Size size,std::size_t count);
int listRowPitch(Size size);
ListLayout scrollingListLayout(Size size,std::size_t count,int offset);
bool validateList(const ListModel& model);
// Models/strings are borrowed and must be stable throughout rendering/gestures.
// A caller updates the model between frames and resets input for rebinding.
class ListScreen final : public Screen {
public:
    explicit ListScreen(const ListModel& model) : model_(model) {}
    ListLayout layout(Size size) const;
    Control rowControl(Size size,std::size_t visibleSlot) const;
    Control footerControl(Size size,std::size_t slot) const;
    void paint(Painter&,Size,const Theme&) const override;
    HitList hits(Size) const override;
private:
    const ListModel& model_;
};
struct DetailField {const char* label;const char* value;bool highlighted=false;};
struct DetailModel {
    const char* title;
    const DetailField* fields;
    std::size_t count;
    std::array<ControlModel,2> footer;
};
struct DetailLayout {
    Rect bounds,title,body;
    std::array<Rect,4> fields{};
    std::array<Rect,2> footer{};
    std::size_t visible=0;
};
DetailLayout detailLayout(Size size,std::size_t fieldCount,bool inset=false);
bool validateDetail(const DetailModel& model);
class DetailScreen final : public Screen {
public:
    explicit DetailScreen(const DetailModel& model,bool inset=false) : model_(model),inset_(inset) {}
    Control footerControl(Size,std::size_t slot) const;
    void paint(Painter&,Size,const Theme&) const override;
    HitList hits(Size) const override;
private:
    const DetailModel& model_;
    bool inset_;
};
// Bounded presentation pages: strings, values, choices and commands are borrowed.
struct MetricCard {const char* label;const char* value;};
struct DashboardModel {
    const char* title;const MetricCard* cards;std::size_t count;
    std::array<ControlModel,2> footer;
};
bool validateDashboard(const DashboardModel&);
class DashboardScreen final : public Screen {
public:
    explicit DashboardScreen(const DashboardModel& model):model_(model){}
    Rect cardBounds(Size,std::size_t slot) const;
    Control footerControl(Size,std::size_t slot) const;
    void paint(Painter&,Size,const Theme&) const override;
    HitList hits(Size) const override;
private:
    const DashboardModel& model_;
};
struct SettingRow {
    const char* label;const char* value;Action action;
    Availability state=Availability::Available;bool selected=false;
};
struct SettingsModel {
    const char* title;const SettingRow* rows;std::size_t count;
    std::array<ControlModel,2> footer;
};
bool validateSettings(const SettingsModel&);
class SettingsScreen final : public Screen {
public:
    explicit SettingsScreen(const SettingsModel& model):model_(model){}
    Rect rowBounds(Size,std::size_t slot) const;
    Control rowControl(Size,std::size_t slot) const;
    Control footerControl(Size,std::size_t slot) const;
    void paint(Painter&,Size,const Theme&) const override;
    HitList hits(Size) const override;
private:
    const SettingsModel& model_;
};
}
