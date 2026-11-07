#include "osd-theme.h"
#include <cassert>
#include <string_view>
int main() {
    using namespace osd;
    assert(&theme()==&classic);
    assert(classic.colors.popup==0x4a49 && classic.colors.menuFrame==0x4a4a);
    assert(focusFill(false)==0x214a && focusFill(true)==0x2b5f);
    assert(theme().metrics.handheldWidth==240 && theme().metrics.handheldHeight==160);
    assert(theme().metrics.handheldRow==10 && theme().metrics.desktopRow==12);
    assert(theme().menuOpacity==255 && !theme().minimalLoading);
    assert(valueColor("yes",0x1234)==0x1234 && valueColor("no",0x1234)==0x1234);
    assert(select("retrorun") && &theme()==&retrorun);
    assert(focusFill(false)==focusFill(true));
    assert(theme().colors.active==rgb565(255,205,64));
    assert(theme().colors.focusSDL==rgb565(195,0,25));
    assert(theme().menuOpacity>0 && theme().menuOpacity<255 && theme().minimalLoading);
    assert(valueColor("yes",theme().colors.focusText)==theme().colors.active);
    assert(valueColor("ON",theme().colors.primary)==theme().colors.active);
    assert(valueColor("no",theme().colors.primary)==theme().colors.disabled);
    assert(valueColor("OFF",theme().colors.primary)==theme().colors.disabled);
    assert(valueColor("Auto",0x1234)==0x1234);
    assert(!select("RETRORUN") && &theme()==&classic);
    assert(select("retrorun"));
    assert(!select("unknown") && &theme()==&classic);
    assert(select("") && &theme()==&classic);
    assert(select("retrorun-blue") && &theme()==&neonBlue);
    assert(focusFill(true)==rgb565(18,105,174) && theme().colors.active==rgb565(185,162,242));
    assert(valueColor("On",0)==theme().colors.active && theme().colors.error==retrorun.colors.error);
    assert(select("retrorun-green") && &theme()==&neonGreen);
    assert(focusFill(false)==rgb565(22,143,76) && theme().colors.active==rgb565(100,184,221));
    assert(theme().metrics.handheldWidth==retrorun.metrics.handheldWidth && theme().minimalLoading);
    assert(selectForLaunch("classic","retrorun-blue") && &theme()==&neonBlue);
    assert(selectForLaunch("retrorun","retrorun-green") && &theme()==&neonGreen);
    assert(selectForLaunch("retrorun-blue","") && &theme()==&neonBlue);
    assert(selectForLaunch("retrorun-green","bad value") && &theme()==&neonGreen);
    assert(selectForLaunch("classic","classic") && &theme()==&classic);
    assert(!selectForLaunch("invalid","") && &theme()==&classic);
}
