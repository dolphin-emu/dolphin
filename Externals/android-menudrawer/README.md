MenuDrawer
==========

A slide-out menu implementation, which allows users to navigate between views
in your app. Most commonly the menu is revealed by either dragging the edge
of the screen, or clicking the 'up' button in the action bar.


Features
--------

 * The menu can be positioned along all four edges.
 * Supports attaching an always visible, non-draggable menu, which is useful
   on e.g. tablets.
 * The menu can wrap both the content and the entire window.
 * Allows the drawer to be opened by dragging the edge, the entire screen or
   not at all.
 * Can be used in XML layouts.
 * Indicator that shows which screen is currently visible.


Usage
=====

This library is very simple to use. It requires no extension of custom classes,
it's simply added to an activity by calling one of the `MenuDrawer#attach(...)`
methods.

For more examples on how to use this library, check out the sample app.


Left menu
---------
```java
public class SampleActivity extends Activity {

    private MenuDrawer mDrawer;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        mDrawer = MenuDrawer.attach(this);
        mDrawer.setContentView(R.layout.activity_sample);
        mDrawer.setMenuView(R.layout.menu_sample);
    }
}
```


Right menu
----------
```java
public class SampleActivity extends Activity {

    private MenuDrawer mDrawer;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        mDrawer = MenuDrawer.attach(this, Position.RIGHT);
        mDrawer.setContentView(R.layout.activity_sample);
        mDrawer.setMenuView(R.layout.menu_sample);
    }
}
```


Credits
=======

 * Cyril Mottier for his [articles][1] on the pattern


License
=======

    Copyright 2012 Simon Vig Therkildsen

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.






 [1]: http://android.cyrilmottier.com/?p=658
