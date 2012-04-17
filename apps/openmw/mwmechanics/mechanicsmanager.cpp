
#include "mechanicsmanager.hpp"

#include <components/esm_store/store.hpp>

#include "../mwgui/window_manager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/environment.hpp"
#include "../mwworld/world.hpp"
#include "../mwworld/player.hpp"

namespace MWMechanics
{
    void MechanicsManager::buildPlayer()
    {
        MWWorld::Ptr ptr = mEnvironment.mWorld->getPlayer().getPlayer();

        MWMechanics::CreatureStats& creatureStats = MWWorld::Class::get (ptr).getCreatureStats (ptr);
        MWMechanics::NpcStats& npcStats = MWWorld::Class::get (ptr).getNpcStats (ptr);

        const ESM::NPC *player = ptr.get<ESM::NPC>()->base;

        // reset
        creatureStats.mLevel = player->npdt52.level;
        creatureStats.mSpells.clear();
        creatureStats.mMagicEffects = MagicEffects();

        for (int i=0; i<27; ++i)
            npcStats.mSkill[i].setBase (player->npdt52.skills[i]);

        // race
        if (mRaceSelected)
        {
            const ESM::Race *race =
                mEnvironment.mWorld->getStore().races.find (
                mEnvironment.mWorld->getPlayer().getRace());

            bool male = mEnvironment.mWorld->getPlayer().isMale();

            for (int i=0; i<8; ++i)
            {
                const ESM::Race::MaleFemale *attribute = 0;
                switch (i)
                {
                    case 0: attribute = &race->data.strength; break;
                    case 1: attribute = &race->data.intelligence; break;
                    case 2: attribute = &race->data.willpower; break;
                    case 3: attribute = &race->data.agility; break;
                    case 4: attribute = &race->data.speed; break;
                    case 5: attribute = &race->data.endurance; break;
                    case 6: attribute = &race->data.personality; break;
                    case 7: attribute = &race->data.luck; break;
                }

                creatureStats.mAttributes[i].setBase (
                    static_cast<int> (male ? attribute->male : attribute->female));
            }

            for (int i=0; i<7; ++i)
            {
                int index = race->data.bonus[i].skill;

                if (index>=0 && index<27)
                {
                    npcStats.mSkill[index].setBase (
                        npcStats.mSkill[index].getBase() + race->data.bonus[i].bonus);
                }
            }

            for (std::vector<std::string>::const_iterator iter (race->powers.list.begin());
                iter!=race->powers.list.end(); ++iter)
            {
                creatureStats.mSpells.add (*iter);
            }
        }

        // birthsign
        if (!mEnvironment.mWorld->getPlayer().getBirthsign().empty())
        {
            const ESM::BirthSign *sign =
                mEnvironment.mWorld->getStore().birthSigns.find (
                mEnvironment.mWorld->getPlayer().getBirthsign());

            for (std::vector<std::string>::const_iterator iter (sign->powers.list.begin());
                iter!=sign->powers.list.end(); ++iter)
            {
                creatureStats.mSpells.add (*iter);
            }
        }

        // class
        if (mClassSelected)
        {
            const ESM::Class& class_ = mEnvironment.mWorld->getPlayer().getClass();

            for (int i=0; i<2; ++i)
            {
                int attribute = class_.data.attribute[i];
                if (attribute>=0 && attribute<8)
                {
                    creatureStats.mAttributes[attribute].setBase (
                        creatureStats.mAttributes[attribute].getBase() + 10);
                }
            }

            for (int i=0; i<2; ++i)
            {
                int bonus = i==0 ? 10 : 25;

                for (int i2=0; i2<5; ++i2)
                {
                    int index = class_.data.skills[i2][i];

                    if (index>=0 && index<27)
                    {
                        npcStats.mSkill[index].setBase (
                            npcStats.mSkill[index].getBase() + bonus);
                    }
                }
            }

            typedef ESMS::IndexListT<ESM::Skill>::MapType ContainerType;
            const ContainerType& skills = mEnvironment.mWorld->getStore().skills.list;

            for (ContainerType::const_iterator iter (skills.begin()); iter!=skills.end(); ++iter)
            {
                if (iter->second.data.specialization==class_.data.specialization)
                {
                    int index = iter->first;

                    if (index>=0 && index<27)
                    {
                        npcStats.mSkill[index].setBase (
                            npcStats.mSkill[index].getBase() + 5);
                    }
                }
            }
        }

        // magic effects
        adjustMagicEffects (ptr);

        // calculate dynamic stats
        int strength = creatureStats.mAttributes[0].getBase();
        int intelligence = creatureStats.mAttributes[1].getBase();
        int willpower = creatureStats.mAttributes[2].getBase();
        int agility = creatureStats.mAttributes[3].getBase();
        int endurance = creatureStats.mAttributes[5].getBase();

        double magickaFactor = creatureStats.mMagicEffects.get (EffectKey (84)).mMagnitude*0.1 + 0.5;

        creatureStats.mDynamic[0].setBase (static_cast<int> (0.5 * (strength + endurance)));
        creatureStats.mDynamic[1].setBase (static_cast<int> (intelligence +
            magickaFactor * intelligence));
        creatureStats.mDynamic[2].setBase (strength+willpower+agility+endurance);

        for (int i=0; i<3; ++i)
            creatureStats.mDynamic[i].setCurrent (creatureStats.mDynamic[i].getModified());
    }

    void MechanicsManager::adjustMagicEffects (MWWorld::Ptr& creature)
    {
        MWMechanics::CreatureStats& creatureStats =
            MWWorld::Class::get (creature).getCreatureStats (creature);

        MagicEffects now = creatureStats.mSpells.getMagicEffects (mEnvironment);

        /// \todo add effects from active spells and equipment

        MagicEffects diff = MagicEffects::diff (creatureStats.mMagicEffects, now);

        creatureStats.mMagicEffects = now;

        // TODO apply diff to other stats
    }

    MechanicsManager::MechanicsManager (MWWorld::Environment& environment)
    : mEnvironment (environment), mUpdatePlayer (true), mClassSelected (false),
      mRaceSelected (false), mActors (environment)
    {
        buildPlayer();
    }

    void MechanicsManager::addActor (const MWWorld::Ptr& ptr)
    {
        mActors.addActor (ptr);
    }

    void MechanicsManager::removeActor (const MWWorld::Ptr& ptr)
    {
        if (ptr==mWatched)
            mWatched = MWWorld::Ptr();

        mActors.removeActor (ptr);
    }

    void MechanicsManager::dropActors (const MWWorld::Ptr::CellStore *cellStore)
    {
        if (!mWatched.isEmpty() && mWatched.getCell()==cellStore)
            mWatched = MWWorld::Ptr();

        mActors.dropActors (cellStore);
    }

    void MechanicsManager::watchActor (const MWWorld::Ptr& ptr)
    {
        mWatched = ptr;
    }

    void MechanicsManager::update (std::vector<std::pair<std::string, Ogre::Vector3> >& movement,
        float duration, bool paused)
    {
        if (!mWatched.isEmpty())
        {
            MWMechanics::CreatureStats& stats =
                MWWorld::Class::get (mWatched).getCreatureStats (mWatched);

            MWMechanics::NpcStats& npcStats =
                MWWorld::Class::get (mWatched).getNpcStats (mWatched);

            static const char *attributeNames[8] =
            {
                "AttribVal1", "AttribVal2", "AttribVal3", "AttribVal4", "AttribVal5",
                "AttribVal6", "AttribVal7", "AttribVal8"
            };

            static const char *dynamicNames[3] =
            {
                "HBar", "MBar", "FBar"
            };

            for (int i=0; i<8; ++i)
            {
                if (stats.mAttributes[i]!=mWatchedCreature.mAttributes[i])
                {
                    mWatchedCreature.mAttributes[i] = stats.mAttributes[i];

                    mEnvironment.mWindowManager->setValue (attributeNames[i], stats.mAttributes[i]);
                }
            }

            for (int i=0; i<3; ++i)
            {
                if (stats.mDynamic[i]!=mWatchedCreature.mDynamic[i])
                {
                    mWatchedCreature.mDynamic[i] = stats.mDynamic[i];

                    mEnvironment.mWindowManager->setValue (dynamicNames[i], stats.mDynamic[i]);
                }
            }

            bool update = false;

            //Loop over ESM::Skill::SkillEnum
            for(int i = 0; i < 27; ++i)
            {
                if(npcStats.mSkill[i] != mWatchedNpc.mSkill[i])
                {
                    update = true;
                    mWatchedNpc.mSkill[i] = npcStats.mSkill[i];
                    mEnvironment.mWindowManager->setValue((ESM::Skill::SkillEnum)i, npcStats.mSkill[i]);
                }
            }

            if (update)
                mEnvironment.mWindowManager->updateSkillArea();

            mEnvironment.mWindowManager->setValue ("level", stats.mLevel);
        }

        if (mUpdatePlayer)
        {
            // basic player profile; should not change anymore after the creation phase is finished.
            mEnvironment.mWindowManager->setValue ("name", mEnvironment.mWorld->getPlayer().getName());
            mEnvironment.mWindowManager->setValue ("race",
                mEnvironment.mWorld->getStore().races.find (mEnvironment.mWorld->getPlayer().
                getRace())->name);
            mEnvironment.mWindowManager->setValue ("class",
                mEnvironment.mWorld->getPlayer().getClass().name);
            mUpdatePlayer = false;

            MWGui::WindowManager::SkillList majorSkills (5);
            MWGui::WindowManager::SkillList minorSkills (5);

            for (int i=0; i<5; ++i)
            {
                minorSkills[i] = mEnvironment.mWorld->getPlayer().getClass().data.skills[i][0];
                majorSkills[i] = mEnvironment.mWorld->getPlayer().getClass().data.skills[i][1];
            }

            mEnvironment.mWindowManager->configureSkills (majorSkills, minorSkills);
        }

        mActors.update (movement, duration, paused);
    }

    void MechanicsManager::setPlayerName (const std::string& name)
    {
        mEnvironment.mWorld->getPlayer().setName (name);
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerRace (const std::string& race, bool male)
    {
        mEnvironment.mWorld->getPlayer().setGender (male);
        mEnvironment.mWorld->getPlayer().setRace (race);
        mRaceSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerBirthsign (const std::string& id)
    {
        mEnvironment.mWorld->getPlayer().setBirthsign (id);
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass (const std::string& id)
    {
        mEnvironment.mWorld->getPlayer().setClass (*mEnvironment.mWorld->getStore().classes.find (id));
        mClassSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass (const ESM::Class& class_)
    {
        mEnvironment.mWorld->getPlayer().setClass (class_);
        mClassSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }
}
