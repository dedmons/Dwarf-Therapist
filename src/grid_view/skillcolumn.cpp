/*
Dwarf Therapist
Copyright (c) 2009 Trey Stout (chmod)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "skillcolumn.h"
#include "gamedatareader.h"
#include "viewcolumnset.h"
#include "columntypes.h"
#include "dwarfmodel.h"
#include "dwarf.h"
#include "dwarfstats.h"
#include "dwarftherapist.h"
#include "skill.h"
#include "trait.h"
#include "viewmanager.h"

SkillColumn::SkillColumn(const QString &title, const int &skill_id, ViewColumnSet *set, QObject *parent, COLUMN_TYPE cType)
    : ViewColumn(title, cType, set, parent)
	, m_skill_id(skill_id)
{
    m_sortable_types << CST_LEVEL << CST_ROLE_RATING << CST_SKILL_RATE;
    m_current_sort = ViewManager::get_default_col_sort(cType);
}

SkillColumn::SkillColumn(QSettings &s, ViewColumnSet *set, QObject *parent) 
    : ViewColumn(s, set, parent)
    , m_skill_id(s.value("skill_id", -1).toInt())
{
    m_sortable_types << CST_LEVEL << CST_ROLE_RATING << CST_SKILL_RATE;
    m_current_sort = ViewManager::get_default_col_sort(CT_SKILL);
}

SkillColumn::SkillColumn(const SkillColumn &to_copy) 
    : ViewColumn(to_copy)
    , m_skill_id(to_copy.m_skill_id)
{
    m_sortable_types = to_copy.m_sortable_types;
}

QStandardItem *SkillColumn::build_cell(Dwarf *d) {
    QStandardItem *item = init_cell(d);    

    item->setData(CT_SKILL, DwarfModel::DR_COL_TYPE);    
    item->setData(d->skill_level(m_skill_id), DwarfModel::DR_DISPLAY_RATING); //level rounded down
    item->setData(d->skill_level(m_skill_id,false,true), DwarfModel::DR_RATING); //interpolated level
    item->setData(m_skill_id, DwarfModel::DR_SKILL_ID);
    item->setData(m_set->name(), DwarfModel::DR_SET_NAME);

    refresh_sort(d, m_current_sort);
    build_tooltip(d,DT->user_settings()->value(QString("options/show_roles_in_skills"),true).toBool());

    return item;

}

void SkillColumn::refresh_sort(COLUMN_SORT_TYPE sType){
    foreach(Dwarf *d, m_cells.uniqueKeys()){        
        refresh_sort(d, sType);
    }
}

void SkillColumn::refresh_sort(Dwarf *d, COLUMN_SORT_TYPE sType){
    if(get_sortable_types().count() > 0){
        if(sType == CST_DEFAULT) //no default sort, use XP
            sType = CST_LEVEL;
        if(!m_sortable_types.contains(sType))
            sType = m_sortable_types.at(0);


        if(m_cells.value(d) && m_cells.value(d)->data(DwarfModel::DR_BASE_SORT).canConvert<float>()){
            m_sort_val = m_cells.value(d)->data(DwarfModel::DR_BASE_SORT).toFloat();
        }

        if(sType == CST_ROLE_RATING){
            if(m_skill_id != -1){
                float m_role_sort_val = 0.0;
                QVector<Role*> related_roles = GameDataReader::ptr()->get_skill_roles().value(m_skill_id);
                if(related_roles.count() > 0){
                    foreach(Role *r, related_roles){
                        m_role_sort_val += d->get_role_rating(r->name);
                    }
                    m_role_sort_val /= related_roles.count();
                    m_sort_val += m_role_sort_val;
                }
            }
        }else if(sType == CST_SKILL_RATE){            
            m_sort_val += d->get_skill(m_skill_id).skill_rate();
        }else{//EXPERIENCE
            m_sort_val += d->skill_level(m_skill_id,true,true);            
        }
    }
    m_cells.value(d)->setData(m_sort_val, DwarfModel::DR_SORT_VALUE);
    m_current_sort = sType;
}

void SkillColumn::build_tooltip(Dwarf *d, bool include_roles){
    GameDataReader *gdr = GameDataReader::ptr();

    //build the role section and adjust the sort value if necessary
    QString role_str="";
    //if(!option_name.isEmpty() && DT->user_settings()->value(QString("options/%1").arg(option_name), true).toBool()) {
    if(include_roles){
        float role_rating=0;
        if(m_skill_id != -1){
            QVector<Role*> found_roles = gdr->get_skill_roles().value(m_skill_id);
            if(found_roles.count() > 0){
                role_rating = 0;
                //just list roles and %
                role_str = tr("<h4>Related Roles:</h4><ul style=\"margin-left:-20px; padding-left:0px;\">");
                foreach(Role *r, found_roles){
                    role_rating = d->get_role_rating(r->name);
                    role_str += tr("<li>%1 (%2%)</li>").arg(r->name).arg(QString::number(role_rating,'f',2));                    
                }
                role_str += "</ul>";
            }
        }
    }

    //skill bonus
    QString str_skill_rate = "";
    if(DT->show_skill_learn_rates){
        int raw_bonus = d->get_skill(m_skill_id).skill_rate();
        int bonus = raw_bonus - 100;
        if(bonus != 0){
            QString prefix = bonus < 0 ? "Penalty" : "Bonus";
            str_skill_rate = tr("<br><b>XP %1: </b>%2% [RAW: %3%]")
                    .arg(prefix)
                    .arg(QString::number(bonus,'f',0))
                    .arg(QString::number(raw_bonus,'f',0));
        }
    }

    QString str_mood = "";
    if(m_skill_id == d->highest_moodable().id()){
        str_mood = tr("<br/><br/>This is the highest moodable skill.");
        if(d->had_mood())
            str_mood = tr("<br/><br/>Had a mood with this skill and crafted '%1'.").arg(d->artifact_name());
    }

    //skill xp, level, name, mood
    QString skill_str = "";
    short rating = d->skill_level(m_skill_id);
    float raw_rating = d->skill_level(m_skill_id, true, true);
    if ((m_skill_id != -1 && rating > -1) || d->had_mood()) {
        if(m_skill_id == -1){
            skill_str = tr("%1 %2")
                    .arg(gdr->get_skill_name(m_skill_id,true))
                    .arg(str_mood);
        }else{
            skill_str = tr("<center><h4 style=\"margin:0;\">%1 %2</h4></center><br/><b>[RAW LEVEL:</b> %3]<br/><b>Experience: </b>%4%5%6")
                    .arg(gdr->get_skill_level_name(rating))
                    .arg(gdr->get_skill_name(m_skill_id,true))
                    .arg(QString::number((int)raw_rating))
                    .arg(d->get_skill(m_skill_id).exp_summary())
                    .arg(str_skill_rate)
                    .arg(str_mood);
        }
    } else {
        // either the skill isn't a valid id, or they have 0 experience in it
        skill_str = "<b>0 Experience</b>";
        skill_str += str_skill_rate;
    }

    QStringList conflicting_traits;
    QString conflicts_str = "";
    QHashIterator<int, Trait*> i(gdr->get_traits());
    while(i.hasNext()){
        i.next();
        QString con = i.value()->conflict_message(m_skill_id, d->trait(i.value()->trait_id));
        if(!con.isEmpty())
            conflicting_traits.append(con);
    }
    if(conflicting_traits.length() > 0){
        conflicts_str = tr("<h4><b>%1</b></h4>")
                .arg(conflicting_traits.join(", "));
    }

    QString tooltip = QString("<center><h3 style=\"margin:0;\">%1</h3></center>%2%3%4%5")
            .arg(m_title)
            .arg(skill_str)
            .arg(role_str)
            .arg(conflicts_str)
            .arg(tooltip_name_footer(d));

    m_cells.value(d)->setToolTip(tooltip);
}

QStandardItem *SkillColumn::build_aggregate(const QString &group_name, const QVector<Dwarf*> &dwarves) {
    Q_UNUSED(dwarves);
    QStandardItem *item = init_aggregate(group_name);
	return item;
}

void SkillColumn::write_to_ini(QSettings &s) {
    ViewColumn::write_to_ini(s);
    s.setValue("skill_id", m_skill_id);
}
