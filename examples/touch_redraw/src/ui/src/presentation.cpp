#include "sample_ui/presentation.h"
#include <algorithm>
namespace sample_ui {
ListLayout listLayout(Size size,std::size_t count,std::size_t first) {
    ListLayout l{};if(!supported(size))return l;
    l.frame=chrome(size);l.viewport=l.frame.body;l.first=first;
    const int rowHeight=size.width>=240 && size.height>=240?44:24;
    const auto& body=l.frame.body;
    const std::size_t slots=std::min<std::size_t>(8,(body.h+l.frame.gap)/(rowHeight+l.frame.gap));
    l.visible=first<count?std::min(slots,count-first):0;
    for(std::size_t i=0;i<l.visible;++i)l.rows[i]={body.x,body.y+static_cast<int>(i)*(rowHeight+l.frame.gap),body.w,rowHeight};
    for(int i=0;i<2;++i)l.footer[i]=cell(l.frame.footer,i,2,1,l.frame.gap);
    return l;
}
int listRowPitch(Size s) {return (s.width>=240 && s.height>=240?44:24)+chrome(s).gap;}
namespace {
Rect scrollViewport(Size s) {
    auto body=chrome(s).body;
    // Bounded row/hit storage even at the maximum supported logical size.
    body.h=std::min(body.h,8*listRowPitch(s)-chrome(s).gap);return body;
}
}
int listScrollLimit(Size s,std::size_t count) {
    if(!supported(s) || !count)return 0;
    const auto pitch=listRowPitch(s);
    const auto bounded=std::min<std::size_t>(count,static_cast<std::size_t>(INT32_MAX/pitch));
    return std::max(0,static_cast<int>(bounded)*pitch-chrome(s).gap-scrollViewport(s).h);
}
ListLayout scrollingListLayout(Size s,std::size_t count,int offset) {
    ListLayout l{};if(!supported(s))return l;
    l.frame=chrome(s);l.viewport=scrollViewport(s);
    const auto body=l.viewport;const int pitch=listRowPitch(s),height=pitch-l.frame.gap;
    offset=std::max(0,std::min(offset,listScrollLimit(s,count)));
    l.first=static_cast<std::size_t>(offset/pitch);
    int y=body.y-offset%pitch;
    if(y+height<=body.y){++l.first;y+=pitch;}
    for(std::size_t i=l.first;i<count && l.visible<l.rows.size() && y<body.y+body.h;++i,y+=pitch)
        l.rows[l.visible++]={body.x,y,body.w-12,height};
    for(int i=0;i<2;++i)l.footer[i]=cell(l.frame.footer,i,2,1,l.frame.gap);
    l.rail={body.x+body.w-6,body.y,6,body.h};
    const int limit=listScrollLimit(s,count);
    const int thumb=limit?std::max(12,static_cast<int>(int64_t(body.h)*body.h/(int64_t(body.h)+limit))):body.h;
    l.thumb={l.rail.x,body.y+(limit?static_cast<int>(int64_t(offset)*(body.h-thumb)/limit):0),6,thumb};
    return l;
}
ListLayout ListScreen::layout(Size s) const {
    return model_.scrollOffset<0?listLayout(s,model_.rows?model_.count:0,model_.first):
        scrollingListLayout(s,model_.rows?model_.count:0,model_.scrollOffset);
}
bool validateList(const ListModel& m) {
    if(!m.title || (m.count && !m.rows))return false;
    for(std::size_t i=0;i<m.count;++i) {
        if(!m.rows[i].id || !m.rows[i].label || !m.rows[i].value || !m.rows[i].action)return false;
        for(std::size_t j=0;j<i;++j)if(m.rows[i].id==m.rows[j].id)return false;
    }
    return true;
}
Control ListScreen::rowControl(Size size,std::size_t slot) const {
    const auto l=layout(size);
    if(slot>=l.visible)return {{},nullptr,0,Availability::Hidden};
    const auto& row=model_.rows[l.first+slot];
    return {intersect(l.rows[slot],l.viewport),row.label,row.action,row.state,row.id==model_.selected,row.id};
}
Control ListScreen::footerControl(Size size,std::size_t slot) const {
    if(slot>=2)return {{},nullptr,0,Availability::Hidden};
    const auto l=listLayout(size,model_.count,model_.first);const auto& f=model_.footer[slot];
    return {l.footer[slot],f.label,f.action,f.state};
}
void ListScreen::paint(Painter& p,Size size,const Theme& t) const {
    if(!supported(size))return;
    const auto l=layout(size);
    p.fill({0,0,size.width,size.height},t.background);p.fill(l.frame.header,t.panel);
    p.text(l.frame.header,model_.title,t.bright);
    if(!l.visible)p.text(l.frame.body,"NO ITEMS",t.muted);
    for(std::size_t i=0;i<l.visible;++i) {
        const auto c=rowControl(size,i);if(c.state==Availability::Hidden)continue;
        const auto& row=model_.rows[l.first+i];
        const bool selected=c.selected && c.state==Availability::Available;
        ClippedPainter clipped(p,l.viewport);Painter& ink=model_.scrollOffset>=0?static_cast<Painter&>(clipped):p;const auto rect=l.rows[i];
        ink.fill(rect,selected?t.accent:t.panel);ink.outline(rect,selected?t.accentHigh:t.border);
        const auto fg=c.state==Availability::Unavailable?t.unavailable:c.state==Availability::Disabled?t.disabled:selected?t.onAccent:t.text;
        const int left=rect.w*3/5;
        ink.text({rect.x+3,rect.y,left-6,rect.h},row.label,fg);
        ink.text({rect.x+left,rect.y,rect.w-left-3,rect.h},row.value,selected?fg:c.state==Availability::Available?t.muted:fg);
    }
    if(model_.scrollOffset>=0) {
        p.fill(l.rail,t.border);p.fill(l.thumb,t.accent);
    }
    for(std::size_t i=0;i<2;++i)footerControl(size,i).paint(p,t);
}
HitList ListScreen::hits(Size size) const {
    HitList h;const auto l=layout(size);
    for(std::size_t i=0;i<l.visible;++i)rowControl(size,i).addHit(h);
    for(std::size_t i=0;i<2;++i)footerControl(size,i).addHit(h);
    return h;
}
bool validateDetail(const DetailModel& m) {
    if(!m.title || m.count>4 || (m.count && !m.fields))return false;
    for(std::size_t i=0;i<m.count;++i)if(!m.fields[i].label || !m.fields[i].value)return false;
    return true;
}
DetailLayout detailLayout(Size size,std::size_t count,bool inset) {
    DetailLayout l{};if(!supported(size))return l;
    l.bounds=inset?Rect{size.width/10,size.height/10,size.width*8/10,size.height*8/10}:Rect{0,0,size.width,size.height};
    const int gap=chrome(size).gap;const auto b=l.bounds;
    const int band=std::max(18,std::min(44,b.h/5));
    l.title={b.x,b.y,b.w,band};l.body={b.x+gap,b.y+band+gap,b.w-2*gap,b.h-2*band-2*gap};
    l.visible=std::min(count,l.fields.size());
    for(std::size_t i=0;i<l.visible;++i)l.fields[i]=cell(l.body,static_cast<int>(i),1,static_cast<int>(l.visible),0);
    const Rect footer{b.x+gap,b.y+b.h-band,b.w-2*gap,band-gap};
    for(int i=0;i<2;++i)l.footer[i]=cell(footer,i,2,1,gap);
    return l;
}
Control DetailScreen::footerControl(Size size,std::size_t slot) const {
    if(slot>=2)return {{},nullptr,0,Availability::Hidden};
    const auto l=detailLayout(size,model_.fields?model_.count:0,inset_);const auto& f=model_.footer[slot];
    return {l.footer[slot],f.label,f.action,f.state};
}
void DetailScreen::paint(Painter& p,Size size,const Theme& t) const {
    if(!supported(size))return;
    const auto l=detailLayout(size,model_.fields?model_.count:0,inset_);
    p.fill(l.bounds,t.background);p.outline(l.bounds,t.border);p.fill(l.title,t.panel);p.text(l.title,model_.title,t.bright);
    p.panel(l.body,t);
    for(std::size_t i=0;i<l.visible;++i) {
        const auto r=l.fields[i];const int left=r.w*2/5;
        const bool highlighted=model_.fields[i].highlighted;
        if(highlighted){p.fill(r,t.accent);p.outline(r,t.accentHigh);}
        p.text({r.x+2,r.y,left-4,r.h},model_.fields[i].label,highlighted?t.onAccent:t.muted);
        p.text({r.x+left,r.y,r.w-left-2,r.h},model_.fields[i].value,highlighted?t.onAccent:t.text);
    }
    for(std::size_t i=0;i<2;++i)footerControl(size,i).paint(p,t);
}
HitList DetailScreen::hits(Size size) const {
    HitList h;for(std::size_t i=0;i<2;++i)footerControl(size,i).addHit(h);return h;
}
namespace {
Control pageFooter(Size s,std::size_t slot,const std::array<ControlModel,2>& footer) {
    if(slot>=footer.size())return {{},nullptr,0,Availability::Hidden};
    const auto c=chrome(s);const auto& f=footer[slot];
    return {cell(c.footer,static_cast<int>(slot),2,1,c.gap),f.label,f.action,f.state};
}
void pageBase(Painter& p,Size s,const Theme& t,const char* title) {
    p.fill({0,0,s.width,s.height},t.background);p.text(chrome(s).header,title,t.bright);
}
}
bool validateDashboard(const DashboardModel& m) {
    if(!m.title || m.count>4 || (m.count && !m.cards))return false;
    for(std::size_t i=0;i<m.count;++i)if(!m.cards[i].label || !m.cards[i].value)return false;
    return true;
}
Rect DashboardScreen::cardBounds(Size s,std::size_t slot) const {
    if(!model_.cards || slot>=std::min<std::size_t>(model_.count,4))return {};
    const auto c=chrome(s);return cell(c.body,static_cast<int>(slot),2,2,c.gap);
}
Control DashboardScreen::footerControl(Size s,std::size_t slot) const {return pageFooter(s,slot,model_.footer);}
void DashboardScreen::paint(Painter& p,Size s,const Theme& t) const {
    pageBase(p,s,t,model_.title);
    for(std::size_t i=0;i<std::min<std::size_t>(model_.count,4) && model_.cards;++i) {
        const auto r=cardBounds(s,i);p.panel(r,t);
        p.text({r.x+3,r.y+1,r.w-6,r.h/3},model_.cards[i].label,t.muted);
        p.text({r.x+3,r.y+r.h/3,r.w-6,r.h*2/3-1},model_.cards[i].value,t.bright,s.width>=240 && r.h>=60?2:1);
    }
    for(std::size_t i=0;i<2;++i)footerControl(s,i).paint(p,t);
}
HitList DashboardScreen::hits(Size s) const {
    HitList h;for(std::size_t i=0;i<2;++i)footerControl(s,i).addHit(h);return h;
}
bool validateSettings(const SettingsModel& m) {
    if(!m.title || m.count>4 || (m.count && !m.rows))return false;
    for(std::size_t i=0;i<m.count;++i)if(!m.rows[i].label || !m.rows[i].value || !m.rows[i].action)return false;
    return true;
}
Rect SettingsScreen::rowBounds(Size s,std::size_t slot) const {
    if(!model_.rows || slot>=std::min<std::size_t>(model_.count,4))return {};
    const auto c=chrome(s);return cell(c.body,static_cast<int>(slot),1,static_cast<int>(std::min<std::size_t>(model_.count,4)),c.gap);
}
Control SettingsScreen::rowControl(Size s,std::size_t slot) const {
    const auto r=rowBounds(s,slot);if(!r.w)return {{},nullptr,0,Availability::Hidden};
    const auto& row=model_.rows[slot];const int split=r.w/2;
    return {{r.x+split,r.y,r.w-split,r.h},row.value,row.action,row.state,row.selected};
}
Control SettingsScreen::footerControl(Size s,std::size_t slot) const {return pageFooter(s,slot,model_.footer);}
void SettingsScreen::paint(Painter& p,Size s,const Theme& t) const {
    pageBase(p,s,t,model_.title);
    for(std::size_t i=0;i<std::min<std::size_t>(model_.count,4) && model_.rows;++i) {
        const auto& row=model_.rows[i];if(row.state==Availability::Hidden)continue;
        const auto r=rowBounds(s,i);p.panel(r,t);
        const auto fg=row.state==Availability::Available?t.text:row.state==Availability::Disabled?t.disabled:t.unavailable;
        p.text({r.x+3,r.y,r.w/2-6,r.h},row.label,fg);rowControl(s,i).paint(p,t);
    }
    for(std::size_t i=0;i<2;++i)footerControl(s,i).paint(p,t);
}
HitList SettingsScreen::hits(Size s) const {
    HitList h;for(std::size_t i=0;i<std::min<std::size_t>(model_.count,4);++i)rowControl(s,i).addHit(h);
    for(std::size_t i=0;i<2;++i)footerControl(s,i).addHit(h);return h;
}
}
