/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/checkbutton.h>

#include "xml/node.h"
#include "../extension.h"
#include "bool.h"
#include "preferences.h"

namespace Inkscape {
namespace Extension {

ParamBool::ParamBool(const gchar * name,
                     const gchar * text,
                     const gchar * description,
                     bool hidden,
                     int indent,
                     Inkscape::Extension::Extension * ext,
                     Inkscape::XML::Node * xml)
    : Parameter(name, text, description, hidden, indent, ext)
    , _value(false)
{
    const char * defaultval = NULL;
    if (xml->firstChild() != NULL) {
        defaultval = xml->firstChild()->content();
    }

    if (defaultval != NULL && (!strcmp(defaultval, "true") || !strcmp(defaultval, "1"))) {
        _value = true;
    } else {
        _value = false;
    }

    gchar * pref_name = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getBool(extension_pref_root + pref_name, _value);
    g_free(pref_name);

    return;
}

bool ParamBool::set( bool in, SPDocument * /*doc*/, Inkscape::XML::Node * /*node*/ )
{
    _value = in;

    gchar * prefname = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setBool(extension_pref_root + prefname, _value);
    g_free(prefname);

    return _value;
}

bool ParamBool::get(const SPDocument * /*doc*/, const Inkscape::XML::Node * /*node*/) const
{
    return _value;
}

/**
 * A check button which is Param aware.  It works with the
 * parameter to change it's value as the check button changes
 * value.
 */
class ParamBoolCheckButton : public Gtk::CheckButton {
public:
    /**
     * Initialize the check button.
     * This function sets the value of the checkbox to be that of the
     * parameter, and then sets up a callback to \c on_toggle.
     *
     * @param  param  Which parameter to adjust on changing the check button
     */
    ParamBoolCheckButton (ParamBool * param, SPDocument * doc, Inkscape::XML::Node * node, sigc::signal<void> * changeSignal) :
            Gtk::CheckButton(), _pref(param), _doc(doc), _node(node), _changeSignal(changeSignal) {
        this->set_active(_pref->get(NULL, NULL) /**\todo fix */);
        this->signal_toggled().connect(sigc::mem_fun(this, &ParamBoolCheckButton::on_toggle));
        return;
    }

    /**
     * A function to respond to the check box changing.
     * Adjusts the value of the preference to match that in the check box.
     */
    void on_toggle (void);

private:
    /** Param to change. */
    ParamBool * _pref;
    SPDocument * _doc;
    Inkscape::XML::Node * _node;
    sigc::signal<void> * _changeSignal;
};

void ParamBoolCheckButton::on_toggle(void)
{
    _pref->set(this->get_active(), NULL /**\todo fix this */, NULL);
    if (_changeSignal != NULL) {
        _changeSignal->emit();
    }
    return;
}

void ParamBool::string(std::string &string) const
{
    if (_value) {
        string += "true";
    } else {
        string += "false";
    }

    return;
}

Gtk::Widget *ParamBool::get_widget(SPDocument * doc, Inkscape::XML::Node * node, sigc::signal<void> * changeSignal)
{
    if (_hidden) {
        return NULL;
    }

    auto hbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, Parameter::GUI_PARAM_WIDGETS_SPACING));
    hbox->set_homogeneous(false);

    Gtk::Label * label = Gtk::manage(new Gtk::Label(_text, Gtk::ALIGN_START));
    label->show();
    hbox->pack_end(*label, true, true);

    ParamBoolCheckButton * checkbox = Gtk::manage(new ParamBoolCheckButton(this, doc, node, changeSignal));
    checkbox->show();
    hbox->pack_start(*checkbox, false, false);

    hbox->show();

    return dynamic_cast<Gtk::Widget *>(hbox);
}

}  /* namespace Extension */
}  /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
