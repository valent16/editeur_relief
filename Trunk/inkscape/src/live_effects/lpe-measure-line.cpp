/*
 * Author(s):
 *   Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 * Some code and ideas migrated from dimensioning.py by
 * Johannes B. Rutzmoser, johannes.rutzmoser (at) googlemail (dot) com
 * https://github.com/Rutzmoser/inkscape_dimensioning
 * Copyright (C) 2014 Author(s)

 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include "live_effects/lpe-measure-line.h"
#include <pangomm/fontdescription.h>
#include "ui/dialog/livepatheffect-editor.h"
#include <libnrtype/font-lister.h>
#include "inkscape.h"
#include "xml/node.h"
#include "xml/sp-css-attr.h"
#include "preferences.h"
#include "util/units.h"
#include "svg/svg-length.h"
#include "svg/svg-color.h"
#include "svg/svg.h"
#include "display/curve.h"
#include "helper/geom.h"
#include "2geom/affine.h"
#include "path-chemistry.h"
#include "style.h"
#include "sp-root.h"
#include "sp-defs.h"
#include "sp-item.h"
#include "sp-shape.h"
#include "sp-path.h"
#include "document.h"
#include <iomanip>

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

using namespace Geom;
namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<OrientationMethod> OrientationMethodData[] = {
    { OM_HORIZONTAL, N_("Horizontal"), "horizontal" }, 
    { OM_VERTICAL, N_("Vertical"), "vertical" },
    { OM_PARALLEL, N_("Parallel"), "parallel" }
};
static const Util::EnumDataConverter<OrientationMethod> OMConverter(OrientationMethodData, OM_END);

LPEMeasureLine::LPEMeasureLine(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    unit(_("Unit"), _("Unit"), "unit", &wr, this, "px"),
    fontbutton(_("Font"), _("Font Selector"), "fontbutton", &wr, this),
    orientation(_("Orientation"), _("Orientation method"), "orientation", OMConverter, &wr, this, OM_PARALLEL, false),
    curve_linked(_("Curve on origin"), _("Curve on origin, set 0 to start/end"), "curve_linked", &wr, this, 1),
    precision(_("Precision"), _("Precision"), "precision", &wr, this, 2),
    position(_("Position"), _("Position"), "position", &wr, this, 5),
    text_top_bottom(_("Text top/bottom"), _("Text top/bottom"), "text_top_bottom", &wr, this, 0),
    text_right_left(_("Text right/left"), _("Text right/left"), "text_right_left", &wr, this, 0),
    helpline_distance(_("Helpline distance"), _("Helpline distance"), "helpline_distance", &wr, this, 0.0),
    helpline_overlap(_("Helpline overlap"), _("Helpline overlap"), "helpline_overlap", &wr, this, 2.0),
    scale(_("Scale"), _("Scaling factor"), "scale", &wr, this, 1.0),
    format(_("Format"), _("Format the number ex:{measure} {unit}, return to save"), "format", &wr, this,"{measure}{unit}"),
    id_origin("id_origin", "id_origin", "id_origin", &wr, this,""),
    arrows_outside(_("Arrows outside"), _("Arrows outside"), "arrows_outside", &wr, this, false),
    flip_side(_("Flip side"), _("Flip side"), "flip_side", &wr, this, false),
    scale_sensitive(_("Scale sensitive"), _("Costrained scale sensitive to transformed containers"), "scale_sensitive", &wr, this, true),
    local_locale(_("Local Number Format"), _("Local number format"), "local_locale", &wr, this, true),
    line_group_05(_("Line Group 0.5"), _("Line Group 0.5, from 0.7"), "line_group_05", &wr, this, true),
    rotate_anotation(_("Rotate Anotation"), _("Rotate Anotation"), "rotate_anotation", &wr, this, true),
    hide_back(_("Hide if label over"), _("Hide DIN line if label over"), "hide_back", &wr, this, true),
    dimline_format(_("CSS DIN line"), _("Override CSS to DIN line, return to save, empty to reset to DIM"), "dimline_format", &wr, this,""),
    helperlines_format(_("CSS helpers"), _("Override CSS to helper lines, return to save, empty to reset to DIM"), "helperlines_format", &wr, this,""),
    anotation_format(_("CSS anotation"), _("Override CSS to anotation text, return to save, empty to reset to DIM"), "anotation_format", &wr, this,""),
    arrows_format(_("CSS arrows"), _("Override CSS to arrows, return to save, empty to reset DIM"), "arrows_format", &wr, this,""),
    expanded(false)
{
    //set to true the parameters you want to be changed his default values
    registerParameter(&unit);
    registerParameter(&fontbutton);
    registerParameter(&orientation);
    registerParameter(&curve_linked);
    registerParameter(&precision);
    registerParameter(&position);
    registerParameter(&text_top_bottom);
    registerParameter(&text_right_left);
    registerParameter(&helpline_distance);
    registerParameter(&helpline_overlap);
    registerParameter(&scale);
    registerParameter(&format);
    registerParameter(&arrows_outside);
    registerParameter(&flip_side);
    registerParameter(&scale_sensitive);
    registerParameter(&local_locale);
    registerParameter(&line_group_05);
    registerParameter(&rotate_anotation);
    registerParameter(&hide_back);
    registerParameter(&dimline_format);
    registerParameter(&helperlines_format);
    registerParameter(&anotation_format);
    registerParameter(&arrows_format);
    registerParameter(&id_origin);

    id_origin.param_hide_canvas_text();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    Glib::ustring format_value = prefs->getString("/live_effects/measure-line/format");
    if(format_value.empty()){
        format_value = "{measure}{unit}";
    }
    format.param_update_default(format_value.c_str());

    format.param_hide_canvas_text();
    dimline_format.param_hide_canvas_text();
    helperlines_format.param_hide_canvas_text();
    anotation_format.param_hide_canvas_text();
    arrows_format.param_hide_canvas_text();
    precision.param_set_range(0, 100);
    precision.param_set_increments(1, 1);
    precision.param_set_digits(0);
    precision.param_make_integer(true);
    curve_linked.param_set_range(0, 999);
    curve_linked.param_set_increments(1, 1);
    curve_linked.param_set_digits(0);
    curve_linked.param_make_integer(true);
    precision.param_make_integer(true);
    position.param_set_range(-999999.0, 999999.0);
    position.param_set_increments(1, 1);
    position.param_set_digits(2);
    text_top_bottom.param_set_range(-999999.0, 999999.0);
    text_top_bottom.param_set_increments(1, 1);
    text_top_bottom.param_set_digits(2);
    text_right_left.param_set_range(-999999.0, 999999.0);
    text_right_left.param_set_increments(1, 1);
    text_right_left.param_set_digits(2);
    helpline_distance.param_set_range(-999999.0, 999999.0);
    helpline_distance.param_set_increments(1, 1);
    helpline_distance.param_set_digits(2);
    helpline_overlap.param_set_range(-999999.0, 999999.0);
    helpline_overlap.param_set_increments(1, 1);
    helpline_overlap.param_set_digits(2);
    start_stored = Geom::Point(0,0);
    end_stored = Geom::Point(0,0);
    id_origin.param_widget_is_visible(false);
}

LPEMeasureLine::~LPEMeasureLine() {}

void
LPEMeasureLine::createArrowMarker(const char * mode)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    SPObject *elemref = NULL;
    Inkscape::XML::Node *arrow = NULL;
    if ((elemref = document->getObjectById(mode))) {
        Inkscape::XML::Node *arrow= elemref->getRepr();
        if (arrow) {
            arrow->setAttribute("sodipodi:insensitive", "true");
            arrow->setAttribute("transform", NULL);
            Inkscape::XML::Node *arrow_data = arrow->firstChild();
            if (arrow_data) {
                SPCSSAttr *css = sp_repr_css_attr_new();
                sp_repr_css_set_property (css, "fill","#000000");
                sp_repr_css_set_property (css, "stroke","none");
                arrow_data->setAttribute("transform", NULL);
                sp_repr_css_attr_add_from_string(css, arrows_format.param_getSVGValue());
                Glib::ustring css_str;
                sp_repr_css_write_string(css,css_str);
                arrow_data->setAttribute("style", css_str.c_str());
            }
        }
    } else {
        arrow = xml_doc->createElement("svg:marker");
        arrow->setAttribute("id", mode);
        arrow->setAttribute("inkscape:stockid", mode);
        arrow->setAttribute("orient", "auto");
        arrow->setAttribute("refX", "0.0");
        arrow->setAttribute("refY", "0.0");
        arrow->setAttribute("style", "overflow:visible");
        arrow->setAttribute("sodipodi:insensitive", "true");
        /* Create <path> */
        Inkscape::XML::Node *arrow_path = xml_doc->createElement("svg:path");
        if (std::strcmp(mode, "ArrowDIN-start") == 0) {
            arrow_path->setAttribute("d", "M -8,0 8,-2.11 8,2.11 z");
        } else if (std::strcmp(mode, "ArrowDIN-end") == 0) {
            arrow_path->setAttribute("d", "M 8,0 -8,2.11 -8,-2.11 z");
        } else if (std::strcmp(mode, "ArrowDINout-start") == 0) {
            arrow_path->setAttribute("d", "M 0,0 -16,2.11 -16,0.5 -26,0.5 -26,-0.5 -16,-0.5 -16,-2.11 z");
        } else {
            arrow_path->setAttribute("d", "M 0,0 16,2.11 16,0.5 26,0.5 26,-0.5 16,-0.5 16,-2.11 z");
        }
        
        arrow_path->setAttribute("id", Glib::ustring(mode).append("_path").c_str());
        SPCSSAttr *css = sp_repr_css_attr_new();
        sp_repr_css_set_property (css, "fill","#000000");
        sp_repr_css_set_property (css, "stroke","none");
        sp_repr_css_attr_add_from_string(css, arrows_format.param_getSVGValue());
        Glib::ustring css_str;
        sp_repr_css_write_string(css,css_str);
        arrow_path->setAttribute("style", css_str.c_str());
        arrow->addChild(arrow_path, NULL);
        Inkscape::GC::release(arrow_path);
        elemref = SP_OBJECT(document->getDefs()->appendChildRepr(arrow));
        Inkscape::GC::release(arrow);
    }
    items.push_back(mode);
}

void
LPEMeasureLine::createTextLabel(Geom::Point pos, double length, Geom::Coord angle, bool remove, bool valid)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    Inkscape::XML::Node *rtext = NULL;
    double doc_w = document->getRoot()->width.value;
    Geom::Scale scale = document->getDocumentScale();
    SPNamedView *nv = sp_document_namedview(document, NULL);
    Glib::ustring display_unit = nv->display_units->abbr;
    if (display_unit.empty()) {
        display_unit = "px";
    }
    //only check constrain viewbox on X
    doc_scale = Inkscape::Util::Quantity::convert( scale[Geom::X], "px", nv->display_units );
    if( doc_scale > 0 ) {
        doc_scale= 1.0/doc_scale;
    } else {
        doc_scale = 1.0;
    }
    const char * id = g_strdup(Glib::ustring("text-on-").append(this->getRepr()->attribute("id")).c_str());
    SPObject *elemref = NULL;
    Inkscape::XML::Node *rtspan = NULL;
    if ((elemref = document->getObjectById(id))) {
        if (remove) {
            elemref->deleteObject();
            return;
        }
        pos = pos - Point::polar(angle, text_right_left);
        rtext = elemref->getRepr();
        sp_repr_set_svg_double(rtext, "x", pos[Geom::X]);
        sp_repr_set_svg_double(rtext, "y", pos[Geom::Y]);
        rtext->setAttribute("sodipodi:insensitive", "true");
        rtext->setAttribute("transform", NULL);
    } else {
        if (remove) {
            return;
        }
        rtext = xml_doc->createElement("svg:text");
        rtext->setAttribute("xml:space", "preserve");
        rtext->setAttribute("id", id);
        rtext->setAttribute("sodipodi:insensitive", "true");
        pos = pos - Point::polar(angle, text_right_left);
        sp_repr_set_svg_double(rtext, "x", pos[Geom::X]);
        sp_repr_set_svg_double(rtext, "y", pos[Geom::Y]);
        rtspan = xml_doc->createElement("svg:tspan");
        rtspan->setAttribute("sodipodi:role", "line");
    }
    gchar * transform;
    Geom::Affine affine = Geom::Affine(Geom::Translate(pos).inverse());
    angle = std::fmod(angle, 2*M_PI);
    if (angle < 0) angle += 2*M_PI;
    if (angle >= rad_from_deg(90) && angle < rad_from_deg(270)) {
        angle = std::fmod(angle + rad_from_deg(180), 2*M_PI);
        if (angle < 0) angle += 2*M_PI;
    }
    affine *= Geom::Rotate(angle);
    affine *= Geom::Translate(pos);
    if (rotate_anotation) {
        transform = sp_svg_transform_write(affine);
    } else {
        transform = NULL;
    }
    rtext->setAttribute("transform", transform);
    g_free(transform);
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_attr_add_from_string(css, anotation_format.param_getSVGValue());
    Inkscape::FontLister *fontlister = Inkscape::FontLister::get_instance();
    fontlister->fill_css(css, Glib::ustring(fontbutton.param_getSVGValue()));
    std::stringstream font_size;
    font_size.imbue(std::locale::classic());
    font_size <<  fontsize << "pt";
    sp_repr_css_set_property (css, "font-size",font_size.str().c_str());
    sp_repr_css_set_property (css, "line-height","125%");
    sp_repr_css_set_property (css, "letter-spacing","0");
    sp_repr_css_set_property (css, "word-spacing", "0");
    sp_repr_css_set_property (css, "text-align", "center");
    sp_repr_css_set_property (css, "text-anchor", "middle");
    sp_repr_css_set_property (css, "fill", "#000000");
    sp_repr_css_set_property (css, "fill-opacity", "1");
    sp_repr_css_set_property (css, "stroke", "none");
    sp_repr_css_attr_add_from_string(css, anotation_format.param_getSVGValue());
    Glib::ustring css_str;
    sp_repr_css_write_string(css,css_str);
    if (!rtspan) {
        rtspan = rtext->firstChild();
    }
    rtext->setAttribute("style", css_str.c_str());
    rtspan->setAttribute("style", NULL);
    rtspan->setAttribute("transform", NULL);
    sp_repr_css_attr_unref (css);
    if (!elemref) {
        rtext->addChild(rtspan, NULL);
        Inkscape::GC::release(rtspan);
    }
    length = Inkscape::Util::Quantity::convert(length / doc_scale, display_unit.c_str(), unit.get_abbreviation());
    char *oldlocale = g_strdup (setlocale(LC_NUMERIC, NULL));
    if (local_locale) {
        setlocale (LC_NUMERIC, "");
    } else {
        setlocale (LC_NUMERIC, "C");
    }
    gchar length_str[64];
    g_snprintf(length_str, 64, "%.*f", (int)precision, length);
    setlocale (LC_NUMERIC, oldlocale);
    g_free (oldlocale);
    Glib::ustring label_value(format.param_getSVGValue());
    size_t s = label_value.find(Glib::ustring("{measure}"),0);
    if(s < label_value.length()) {
        label_value.replace(s,s+9,length_str);
    }
    s = label_value.find(Glib::ustring("{unit}"),0);
    if(s < label_value.length()) {
        label_value.replace(s,s+6,unit.get_abbreviation());
    }
    if ( !valid ) {
        label_value = Glib::ustring(_("Non Uniform Scale"));
    }
    Inkscape::XML::Node *rstring = NULL;
    if (!elemref) {
        rstring = xml_doc->createTextNode(label_value.c_str());
        rtspan->addChild(rstring, NULL);
        Inkscape::GC::release(rstring);
    } else {
        rstring = rtspan->firstChild();
        rstring->setContent(label_value.c_str());
    }
    if (!elemref) {
        elemref = sp_lpe_item->parent->appendChildRepr(rtext);
        Inkscape::GC::release(rtext);
    } else if (elemref->parent != sp_lpe_item->parent) {
        Inkscape::XML::Node *old_repr = elemref->getRepr();
        Inkscape::XML::Node *copy = old_repr->duplicate(xml_doc);
        SPObject * elemref_copy = sp_lpe_item->parent->appendChildRepr(copy);
        Inkscape::GC::release(copy);
        elemref->deleteObject();
        copy->setAttribute("id", id);
        elemref = elemref_copy;
    }
    items.push_back(id);
    Geom::OptRect bounds = SP_ITEM(elemref)->bounds(SPItem::GEOMETRIC_BBOX);
    if (bounds) {
        anotation_width = bounds->width() * 1.15;
    }
}

void
LPEMeasureLine::createLine(Geom::Point start,Geom::Point end, const char * id, bool main, bool overflow, bool remove, bool arrows)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    SPObject *elemref = NULL;
    Inkscape::XML::Node *line = NULL;
    if (!main) {
        Geom::Ray ray(start, end);
        Geom::Coord angle = ray.angle();
        start = start + Point::polar(angle, helpline_distance );
        end = end + Point::polar(angle, helpline_overlap );
    }
    Geom::PathVector line_pathv;
    if (main && std::abs(text_top_bottom) < fontsize/1.5 && hide_back && !overflow){
        Geom::Path line_path;
        double k = 0;
        if (flip_side) {
            k = (Geom::distance(start,end)/2.0) + arrow_gap - (anotation_width/2.0);
        } else {
            k = (Geom::distance(start,end)/2.0) - arrow_gap - (anotation_width/2.0);
        }
        if (Geom::distance(start,end) < anotation_width){
            if ((elemref = document->getObjectById(id))) {
                if (remove) {
                    elemref->deleteObject();
                }
                return;
            }
        }
        //k = std::max(k , arrow_gap -1);
        Geom::Ray ray(end, start);
        Geom::Coord angle = ray.angle();
        line_path.start(start);
        line_path.appendNew<Geom::LineSegment>(start - Point::polar(angle, k));
        line_pathv.push_back(line_path);
        line_path.clear();
        line_path.start(end + Point::polar(angle, k));
        line_path.appendNew<Geom::LineSegment>(end);
        line_pathv.push_back(line_path);
    } else {
        Geom::Path line_path;
        line_path.start(start);
        line_path.appendNew<Geom::LineSegment>(end);
        line_pathv.push_back(line_path);
    }
    if ((elemref = document->getObjectById(id))) {
        if (remove) {
            elemref->deleteObject();
            return;
        }
        line = elemref->getRepr();
       
        gchar * line_str = sp_svg_write_path( line_pathv );
        line->setAttribute("d" , line_str);
        line->setAttribute("transform", NULL);
        g_free(line_str);
    } else {
        if (remove) {
            return;
        }
        line = xml_doc->createElement("svg:path");
        line->setAttribute("id", id);
        gchar * line_str = sp_svg_write_path( line_pathv );
        line->setAttribute("d" , line_str);
        g_free(line_str);
    }
    line->setAttribute("sodipodi:insensitive", "true");
    line_pathv.clear();
        
    Glib::ustring style = Glib::ustring("stroke:#000000;fill:none;");
    if (overflow && !arrows) {
        line->setAttribute("inkscape:label", "downline");
    } else if (main) {
        line->setAttribute("inkscape:label", "dinline");
        if (arrows_outside) {
            style = style + Glib::ustring("marker-start:url(#ArrowDINout-start);marker-end:url(#ArrowDINout-end);");
        } else {
            style = style + Glib::ustring("marker-start:url(#ArrowDIN-start);marker-end:url(#ArrowDIN-end);");
        }
    } else {
        line->setAttribute("inkscape:label", "dinhelpline");
    }
    std::stringstream stroke_w;
    stroke_w.imbue(std::locale::classic());
    if (line_group_05) {
        double stroke_width = Inkscape::Util::Quantity::convert(0.25 / doc_scale, "mm", display_unit.c_str());
        stroke_w <<  stroke_width;
        style = style + Glib::ustring("stroke-width:" + stroke_w.str());
    } else {
        double stroke_width = Inkscape::Util::Quantity::convert(0.35 / doc_scale, "mm", display_unit.c_str());
        stroke_w <<  stroke_width;
        style = style + Glib::ustring("stroke-width:" + stroke_w.str());
    }
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_attr_add_from_string(css, style.c_str());
    if (main) {
        sp_repr_css_attr_add_from_string(css, dimline_format.param_getSVGValue());
    } else {
        sp_repr_css_attr_add_from_string(css, helperlines_format.param_getSVGValue());
    }
    Glib::ustring css_str;
    sp_repr_css_write_string(css,css_str);
    line->setAttribute("style", css_str.c_str());
    if (!elemref) {
        elemref = sp_lpe_item->parent->appendChildRepr(line);
        Inkscape::GC::release(line);
    } else if (elemref->parent != sp_lpe_item->parent) {
        Inkscape::XML::Node *old_repr = elemref->getRepr();
        Inkscape::XML::Node *copy = old_repr->duplicate(xml_doc);
        SPObject * elemref_copy = sp_lpe_item->parent->appendChildRepr(copy);
        Inkscape::GC::release(copy);
        elemref->deleteObject();
        copy->setAttribute("id", id);
    }
    items.push_back(id);
}

void
LPEMeasureLine::doOnApply(SPLPEItem const* lpeitem)
{
    if (!SP_IS_SHAPE(lpeitem)) {
        g_warning("LPE measure line can only be applied to shapes (not groups).");
        SPLPEItem * item = const_cast<SPLPEItem*>(lpeitem);
        item->removeCurrentPathEffect(false);
    }
    id_origin.param_setValue(Glib::ustring(lpeitem->getId()));
    id_origin.write_to_SVG();
}

void
LPEMeasureLine::doBeforeEffect (SPLPEItem const* lpeitem)
{
    SPLPEItem * splpeitem = const_cast<SPLPEItem *>(lpeitem);
    sp_lpe_item->parent = dynamic_cast<SPObject *>(splpeitem->parent);
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Node *root = splpeitem->document->getReprRoot();
    Inkscape::XML::Node *root_origin = document->getReprRoot();
    if (root_origin != root) {
        return;
    }
    SPPath *sp_path = dynamic_cast<SPPath *>(splpeitem);
    if (sp_path) {
        Geom::Affine affinetransform = i2anc_affine(SP_OBJECT(lpeitem), SP_OBJECT(document->getRoot()));
        Geom::PathVector pathvector = sp_path->get_original_curve()->get_pathvector();
        Geom::Affine writed_transform = Geom::identity();
        sp_svg_transform_read(splpeitem->getAttribute("transform"), &writed_transform );
        pathvector *= writed_transform;
        if ((Glib::ustring(format.param_getSVGValue()).empty())) {
            format.param_setValue(Glib::ustring("{measure}{unit}"));
        }
        size_t ncurves = pathvector.curveCount();
        if (ncurves != (size_t)curve_linked.param_get_max()) {
            curve_linked.param_set_range(0, ncurves);
        }
        Geom::Point start = pathvector.initialPoint();
        Geom::Point end =  pathvector.finalPoint();
        if (curve_linked) { //!0 
            start = pathvector.pointAt(curve_linked -1);
            end = pathvector.pointAt(curve_linked);
        }
        if (Geom::are_near(start, start_stored, 0.01) && 
            Geom::are_near(end, end_stored, 0.01) &&
            sp_lpe_item->getCurrentLPE() != this){
            return;
        }
        items.clear();
        start_stored = start;
        end_stored = end;
        Geom::Point hstart = start;
        Geom::Point hend = end;
        bool remove = false;
        if (Geom::are_near(hstart, hend)) {
            remove = true;
        }
        if (orientation == OM_VERTICAL) {
            Coord xpos = std::max(hstart[Geom::X],hend[Geom::X]);
            if (flip_side) {
                xpos = std::min(hstart[Geom::X],hend[Geom::X]);
            }
            hstart[Geom::X] = xpos;
            hend[Geom::X] = xpos;
            if (hstart[Geom::Y] > hend[Geom::Y]) {
                swap(hstart,hend);
                swap(start,end);
            }
            if (Geom::are_near(hstart[Geom::Y], hend[Geom::Y])) {
                remove = true;
            }
        }
        if (orientation == OM_HORIZONTAL) {
            Coord ypos = std::max(hstart[Geom::Y],hend[Geom::Y]);
            if (flip_side) {
                ypos = std::min(hstart[Geom::Y],hend[Geom::Y]);
            }
            hstart[Geom::Y] = ypos;
            hend[Geom::Y] = ypos;
            if (hstart[Geom::X] < hend[Geom::X]) {
                swap(hstart,hend);
                swap(start,end);
            }
            if (Geom::are_near(hstart[Geom::X], hend[Geom::X])) {
                remove = true;
            }
        }
        double length = Geom::distance(hstart,hend)  * scale;
        Geom::Point pos = Geom::middle_point(hstart,hend);
        Geom::Ray ray(hstart,hend);
        Geom::Coord angle = ray.angle();
        if (flip_side) {
            angle = std::fmod(angle + rad_from_deg(180), 2*M_PI);
            if (angle < 0) angle += 2*M_PI;
        }
        if (arrows_outside) {
            createArrowMarker("ArrowDINout-start");
            createArrowMarker("ArrowDINout-end");
        } else {
            createArrowMarker("ArrowDIN-start");
            createArrowMarker("ArrowDIN-end");
        }
        //We get the font size to offset the text to the middle
        Pango::FontDescription fontdesc(Glib::ustring(fontbutton.param_getSVGValue()));
        fontsize = fontdesc.get_size()/(double)Pango::SCALE;
        fontsize *= document->getRoot()->c2p.inverse().expansionX();
        Geom::Coord angle_cross = std::fmod(angle + rad_from_deg(90), 2*M_PI);
        if (angle_cross < 0) angle_cross += 2*M_PI;
        angle = std::fmod(angle, 2*M_PI);
        if (angle < 0) angle += 2*M_PI;
        if (angle >= rad_from_deg(90) && angle < rad_from_deg(270)) {
            pos = pos - Point::polar(angle_cross, (position - text_top_bottom) + fontsize/2.5);
        } else {
            pos = pos - Point::polar(angle_cross, (position + text_top_bottom) - fontsize/2.5);
        }
        double parents_scale = (affinetransform.expansionX() + affinetransform.expansionY()) / 2.0;
        if (scale_sensitive) {
            length *= parents_scale;
        }
        if (scale_sensitive && !affinetransform.preservesAngles()) {
            createTextLabel(pos, length, angle, remove, false);
        } else {
            createTextLabel(pos, length, angle, remove, true);
        }
        bool overflow = false;
        const char * downline = g_strdup(Glib::ustring("downline-").append(this->getRepr()->attribute("id")).c_str());
        //delete residual lines if exist
        createLine(Geom::Point(),Geom::Point(), downline, true, overflow, true, false);
        //Create it 
        if ((anotation_width/2) + std::abs(text_right_left) > Geom::distance(start,end)/2.0) {
            Geom::Point sstart = end - Point::polar(angle_cross, position);
            Geom::Point send = end - Point::polar(angle_cross, position);
            if ((text_right_left < 0 && flip_side) || (text_right_left > 0 && !flip_side)) {
                sstart = start - Point::polar(angle_cross, position);
                send = start - Point::polar(angle_cross, position);
            }
            Geom::Point prog_end = Geom::Point();
            if (std::abs(text_top_bottom) < fontsize/1.5 && hide_back) { 
                if (text_right_left > 0 ) {
                    prog_end = sstart - Point::polar(angle, std::abs(text_right_left) - (anotation_width/1.9) - (Geom::distance(start,end)/2.0));
                } else {
                    prog_end = sstart + Point::polar(angle, std::abs(text_right_left) - (anotation_width/1.9) - (Geom::distance(start,end)/2.0));
                }
            } else {
                if (text_right_left > 0 ) {
                    prog_end = sstart - Point::polar(angle,(anotation_width/2) + std::abs(text_right_left) - (Geom::distance(start,end)/2.0));
                } else {
                    prog_end = sstart + Point::polar(angle,(anotation_width/2) + std::abs(text_right_left) - (Geom::distance(start,end)/2.0));
                }
            }
            overflow = true;
            createLine(sstart, prog_end, downline, true, overflow, false, false);
        }
        //LINE
        arrow_gap = 8 * Inkscape::Util::Quantity::convert(0.35 / doc_scale, "mm", display_unit.c_str());
        if (line_group_05) {
            arrow_gap = 8 * Inkscape::Util::Quantity::convert(0.25 / doc_scale, "mm", display_unit.c_str());
        }
        SPCSSAttr *css = sp_repr_css_attr_new();
        sp_repr_css_attr_add_from_string(css, dimline_format.param_getSVGValue());
        char *oldlocale = g_strdup (setlocale(LC_NUMERIC, NULL));
        setlocale (LC_NUMERIC, "C");
        double width_line =  atof(sp_repr_css_property(css,"stroke-width","-1"));
        setlocale (LC_NUMERIC, oldlocale);
        g_free (oldlocale);
        if (width_line > -0.0001) {
             arrow_gap = 8 * Inkscape::Util::Quantity::convert(width_line/ doc_scale, "mm", display_unit.c_str());
        }
        if (flip_side) {
            arrow_gap *= -1;
        }
        hstart = hstart - Point::polar(angle_cross, position);
        createLine(start, hstart, g_strdup(Glib::ustring("infoline-on-start-").append(this->getRepr()->attribute("id")).c_str()), false, false, remove);
        hend = hend - Point::polar(angle_cross, position);
        createLine(end, hend, g_strdup(Glib::ustring("infoline-on-end-").append(this->getRepr()->attribute("id")).c_str()), false, false, remove);
        if (!arrows_outside) {
            hstart = hstart + Point::polar(angle, arrow_gap);
            hend = hend - Point::polar(angle, arrow_gap );
        }
        createLine(hstart, hend, g_strdup(Glib::ustring("infoline-").append(this->getRepr()->attribute("id")).c_str()), true, overflow, remove, true);     
    }
}

void
LPEMeasureLine::doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/)
{
    processObjects(LPE_VISIBILITY);
}

void 
LPEMeasureLine::doOnRemove (SPLPEItem const* /*lpeitem*/)
{
    //set "keep paths" hook on sp-lpe-item.cpp
    if (keep_paths) {
        processObjects(LPE_TO_OBJECTS);
        items.clear();
        return;
    }
    processObjects(LPE_ERASE);
}

Gtk::Widget *LPEMeasureLine::newWidget()
{
    // use manage here, because after deletion of Effect object, others might
    // still be pointing to this widget.
    Gtk::VBox *vbox = Gtk::manage(new Gtk::VBox(Effect::newWidget()));

    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(2);

    std::vector<Parameter *>::iterator it = param_vector.begin();
    Gtk::VBox * vbox_expander = Gtk::manage( new Gtk::VBox(Effect::newWidget()) );
    vbox_expander->set_border_width(0);
    vbox_expander->set_spacing(2);
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter *param = *it;
            Gtk::Widget *widg = dynamic_cast<Gtk::Widget *>(param->param_newWidget());
            Glib::ustring *tip = param->param_getTooltip();
            if (widg) {
                if (param->param_key != "dimline_format" &&
                    param->param_key != "helperlines_format" &&
                    param->param_key != "arrows_format" &&
                    param->param_key != "anotation_format") {
                    vbox->pack_start(*widg, true, true, 2);
                } else {
                    vbox_expander->pack_start(*widg, true, true, 2);
                }
                if (tip) {
                    widg->set_tooltip_text(*tip);
                } else {
                    widg->set_tooltip_text("");
                    widg->set_has_tooltip(false);
                }
            }
        }

        ++it;
    }
    expander = Gtk::manage(new Gtk::Expander(Glib::ustring(_("Show DIM CSS style override"))));
    expander->add(*vbox_expander);
    expander->set_expanded(expanded);
    expander->property_expanded().signal_changed().connect(sigc::mem_fun(*this, &LPEMeasureLine::onExpanderChanged) );
    vbox->pack_start(*expander, true, true, 2);
    return dynamic_cast<Gtk::Widget *>(vbox);
}

void
LPEMeasureLine::onExpanderChanged()
{
    expanded = expander->get_expanded();
    if(expanded) {
        expander->set_label (Glib::ustring(_("Hide DIM CSS style override")));
    } else {
        expander->set_label (Glib::ustring(_("Show DIM CSS style override")));
    }
}

Geom::PathVector
LPEMeasureLine::doEffect_path(Geom::PathVector const &path_in)
{
    return path_in;
}

}; //namespace LivePathEffect
}; /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offset:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
