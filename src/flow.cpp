#include "flow.h"
#include "Scene.h"
#include "Transition.h"
#include "DebugActor.h"

namespace oxygine
{
    bool checkQuit();

    namespace flow
    {
        spActor _touchBlocker;

        Vector2 _blockedTouchPosition(0, 0);
        bool _wasTouchBlocked = false;
        bool _wasBackBlocked = false;


        Flow Flow::instance;




        void init()
        {
            _touchBlocker = new Actor;
            _touchBlocker->setName("Scene::_touchBlocker");
            _touchBlocker->setPosition(-10000, -10000);
            _touchBlocker->setSize(20000, 20000);
            _touchBlocker->setPriority(10000);
            _touchBlocker->setName("touchBlocker");

            Flow::instance.init();
        }

        void free()
        {
            _touchBlocker = 0;
        }


        Flow::Flow()
        {
            _transition = false;
            _back = false;
        }

        void Flow::init()
        {
            spScene first = new Scene;
            first->_holder->attachTo(getStage());
            scenes.push_back(first);
        }

        void Flow::show(spScene scene, const resultCallback& cb)
        {
			auto p = std::find(scenes.begin(), scenes.end(), scene);
			if (p != scenes.end())			
				log::error("you can't show scene '%s', it is already in the list", scene->getName());

			OX_ASSERT(p == scenes.end());
            scene->_resultCB = cb;
            scenes2show.push_back(scene);

            checkShow();
        }

        void Flow::blockedTouch(Event* ev)
        {
            TouchEvent* event = safeCast<TouchEvent*>(ev);
            _blockedTouchPosition = event->getPointer()->getPosition();
            _wasTouchBlocked = true;
        }

        void Flow::phaseBegin(spScene current, spScene next, bool back)
        {
            _back = back;
            _current = current;
            _next = next;

            _transition = true;

            _trans = next->runTransition(this, current, back);

            if (next && !_back)
                next->entering();

            if (!_back || !current->_dialog)
                next->preShowing();

            if ((next->_dialog && _back) || !next->_dialog)
                current->preHiding();

            if (!back)
                current->sceneShown(next);

            getStage()->addChild(_touchBlocker);
            _wasTouchBlocked = false;
            _wasBackBlocked = false;
            getStage()->addEventListener(TouchEvent::CLICK, CLOSURE(this, &Flow::blockedTouch));
        }

        void Flow::phaseEnd()
        {
            _transition = false;

            spScene current = _current;
            spScene next = _next;

            _current = 0;
            _next = 0;

            if ((next->_dialog && _back) || !next->_dialog)
            {
                current->_holder->detach();
                current->postHiding();
            }

            getStage()->insertChildBefore(_touchBlocker, next->getHolder());

            if (!_back || !current->_dialog)
                next->postShowing();

            if (_back)
                next->sceneHidden(current);

            getStage()->removeEventListener(TouchEvent::CLICK, CLOSURE(this, &Flow::blockedTouch));

            if (current->_done)
            {
                current->leaving();
                if (current->_resultCB)
                {
                    current->_resultCB(&current->_finishEvent);
                    current->_resultCB = resultCallback();
                }
            }

            if (current->_remove)
            {
                OX_ASSERT(next->_dialog == false);
                std::vector<spScene>::iterator i = std::find(scenes.begin(), scenes.end(), current);
                OX_ASSERT(i != scenes.end());
                scenes.erase(i);
            }


            if (_wasTouchBlocked)
            {
                log::messageln("send  blocked touch");
                TouchEvent click(TouchEvent::CLICK, true, _blockedTouchPosition);
                getStage()->handleEvent(&click);
                _wasTouchBlocked = false;
            }
        }

        void Flow::checkDone()
        {
            spScene current = scenes.back();
            if (current->_done)
            {
                scenes.pop_back();
                spScene prev = scenes.back();

                phaseBegin(current, prev, true);
            }
        }

        void Flow::checkShow()
        {
            if (scenes.back()->_done)
                return;

            if (_transition)
                return;


            spScene next = scenes2show.front();
            scenes2show.erase(scenes2show.begin());

            spScene current = scenes.back();

            if (current->_dialog)
            {
                OX_ASSERT(next->_dialog && "you can't show fullscreen scene on top of dialog");
                OX_ASSERT(!current->_remove && "you can't remove dialog from flow");
            }

            scenes.push_back(next);
            phaseBegin(current, next, false);

        }

        void Flow::update()
        {
            if (DebugActor::instance)
            {
                std::string str;
                for (size_t i = 1; i < scenes.size(); ++i)
                {
                    str += scenes[i]->getName();
                    str += "->";
                }
                if (!str.empty())
                {
                    str.pop_back();
                    str.pop_back();
                    str += "\n";
                    DebugActor::addDebugString(str.c_str());
                }
            }

            if (checkQuit())
                _wasBackBlocked = true;

            if (_transition)
            {
                if (_current->_transitionDone || (_trans && _trans->_done))
                    phaseEnd();

                return;
            }

            checkDone();
            if (!scenes2show.empty())
                checkShow();

            if (_transition)
                return;

            if (_wasBackBlocked && !scenes.empty())
            {
                _wasBackBlocked = false;
                spScene current = scenes.back();

                Event ev(Scene::EVENT_BACK);
                current->dispatchEvent(&ev);
                checkDone();
            }
        }

        void update()
        {
            Flow::instance.update();
        }

        void show(spScene s, const resultCallback& cb, Flow* f)
        {
            if (!f)
                f = &Flow::instance;

            f->show(s, cb);
        }
    }
}