package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewTreeObserver;
import android.view.animation.Interpolator;

public abstract class MenuDrawer extends ViewGroup {

    /**
     * Callback interface for changing state of the drawer.
     */
    public interface OnDrawerStateChangeListener {

        /**
         * Called when the drawer state changes.
         *
         * @param oldState The old drawer state.
         * @param newState The new drawer state.
         */
        void onDrawerStateChange(int oldState, int newState);
    }

    /**
     * Callback that is invoked when the drawer is in the process of deciding whether it should intercept the touch
     * event. This lets the listener decide if the pointer is on a view that would disallow dragging of the drawer.
     * This is only called when the touch mode is {@link #TOUCH_MODE_FULLSCREEN}.
     */
    public interface OnInterceptMoveEventListener {

        /**
         * Called for each child the pointer i on when the drawer is deciding whether to intercept the touch event.
         *
         * @param v  View to test for draggability
         * @param dx Delta drag in pixels
         * @param x  X coordinate of the active touch point
         * @param y  Y coordinate of the active touch point
         * @return true if view is draggable by delta dx.
         */
        boolean isViewDraggable(View v, int dx, int x, int y);
    }

    /**
     * Tag used when logging.
     */
    private static final String TAG = "MenuDrawer";

    /**
     * Indicates whether debug code should be enabled.
     */
    private static final boolean DEBUG = false;

    /**
     * The time between each frame when animating the drawer.
     */
    protected static final int ANIMATION_DELAY = 1000 / 60;

    /**
     * The default touch bezel size of the drawer in dp.
     */
    private static final int DEFAULT_DRAG_BEZEL_DP = 24;

    /**
     * The default drop shadow size in dp.
     */
    private static final int DEFAULT_DROP_SHADOW_DP = 6;

    /**
     * Drag mode for sliding only the content view.
     */
    public static final int MENU_DRAG_CONTENT = 0;

    /**
     * Drag mode for sliding the entire window.
     */
    public static final int MENU_DRAG_WINDOW = 1;

    /**
     * Disallow opening the drawer by dragging the screen.
     */
    public static final int TOUCH_MODE_NONE = 0;

    /**
     * Allow opening drawer only by dragging on the edge of the screen.
     */
    public static final int TOUCH_MODE_BEZEL = 1;

    /**
     * Allow opening drawer by dragging anywhere on the screen.
     */
    public static final int TOUCH_MODE_FULLSCREEN = 2;

    /**
     * Indicates that the drawer is currently closed.
     */
    public static final int STATE_CLOSED = 0;

    /**
     * Indicates that the drawer is currently closing.
     */
    public static final int STATE_CLOSING = 1;

    /**
     * Indicates that the drawer is currently being dragged by the user.
     */
    public static final int STATE_DRAGGING = 2;

    /**
     * Indicates that the drawer is currently opening.
     */
    public static final int STATE_OPENING = 4;

    /**
     * Indicates that the drawer is currently open.
     */
    public static final int STATE_OPEN = 8;

    /**
     * Indicates whether to use {@link View#setTranslationX(float)} when positioning views.
     */
    static final boolean USE_TRANSLATIONS = Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1;

    /**
     * Time to animate the indicator to the new active view.
     */
    static final int INDICATOR_ANIM_DURATION = 800;

    /**
     * The maximum animation duration.
     */
    private static final int DEFAULT_ANIMATION_DURATION = 600;

    /**
     * Interpolator used when animating the drawer open/closed.
     */
    protected static final Interpolator SMOOTH_INTERPOLATOR = new SmoothInterpolator();

    /**
     * Drawable used as menu overlay.
     */
    protected Drawable mMenuOverlay;

    /**
     * Defines whether the drop shadow is enabled.
     */
    protected boolean mDropShadowEnabled;

    /**
     * Drawable used as content drop shadow onto the menu.
     */
    protected Drawable mDropShadowDrawable;

    /**
     * The size of the content drop shadow.
     */
    protected int mDropShadowSize;

    /**
     * Bitmap used to indicate the active view.
     */
    protected Bitmap mActiveIndicator;

    /**
     * The currently active view.
     */
    protected View mActiveView;

    /**
     * Position of the active view. This is compared to View#getTag(R.id.mdActiveViewPosition) when drawing the
     * indicator.
     */
    protected int mActivePosition;

    /**
     * Whether the indicator should be animated between positions.
     */
    private boolean mAllowIndicatorAnimation;

    /**
     * Used when reading the position of the active view.
     */
    protected final Rect mActiveRect = new Rect();

    /**
     * Temporary {@link Rect} used for deciding whether the view should be invalidated so the indicator can be redrawn.
     */
    private final Rect mTempRect = new Rect();

    /**
     * The custom menu view set by the user.
     */
    private View mMenuView;

    /**
     * The parent of the menu view.
     */
    protected BuildLayerFrameLayout mMenuContainer;

    /**
     * The parent of the content view.
     */
    protected BuildLayerFrameLayout mContentContainer;

    /**
     * The size of the menu (width or height depending on the gravity).
     */
    protected int mMenuSize;

    /**
     * Indicates whether the menu size has been set explicity either via the theme or by calling
     * {@link #setMenuSize(int)}.
     */
    protected boolean mMenuSizeSet;

    /**
     * Indicates whether the menu is currently visible.
     */
    protected boolean mMenuVisible;

    /**
     * The drag mode of the drawer. Can be either {@link #MENU_DRAG_CONTENT} or {@link #MENU_DRAG_WINDOW}.
     */
    private int mDragMode = MENU_DRAG_CONTENT;

    /**
     * The current drawer state.
     *
     * @see #STATE_CLOSED
     * @see #STATE_CLOSING
     * @see #STATE_DRAGGING
     * @see #STATE_OPENING
     * @see #STATE_OPEN
     */
    protected int mDrawerState = STATE_CLOSED;

    /**
     * The touch bezel size of the drawer in px.
     */
    protected int mTouchBezelSize;

    /**
     * The touch area size of the drawer in px.
     */
    protected int mTouchSize;

    /**
     * Listener used to dispatch state change events.
     */
    private OnDrawerStateChangeListener mOnDrawerStateChangeListener;

    /**
     * Touch mode for the Drawer.
     * Possible values are {@link #TOUCH_MODE_NONE}, {@link #TOUCH_MODE_BEZEL} or {@link #TOUCH_MODE_FULLSCREEN}
     * Default: {@link #TOUCH_MODE_BEZEL}
     */
    protected int mTouchMode = TOUCH_MODE_BEZEL;

    /**
     * Indicates whether to use {@link View#LAYER_TYPE_HARDWARE} when animating the drawer.
     */
    protected boolean mHardwareLayersEnabled = true;

    /**
     * The Activity the drawer is attached to.
     */
    private Activity mActivity;

    /**
     * Scroller used when animating the indicator to a new position.
     */
    private FloatScroller mIndicatorScroller;

    /**
     * Runnable used when animating the indicator to a new position.
     */
    private Runnable mIndicatorRunnable = new Runnable() {
        @Override
        public void run() {
            animateIndicatorInvalidate();
        }
    };

    /**
     * The start position of the indicator when animating it to a new position.
     */
    protected int mIndicatorStartPos;

    /**
     * [0..1] value indicating the current progress of the animation.
     */
    protected float mIndicatorOffset;

    /**
     * Whether the indicator is currently animating.
     */
    protected boolean mIndicatorAnimating;

    /**
     * Bundle used to hold the drawers state.
     */
    protected Bundle mState;

    /**
     * The maximum duration of open/close animations.
     */
    protected int mMaxAnimationDuration = DEFAULT_ANIMATION_DURATION;

    /**
     * Callback that lets the listener override intercepting of touch events.
     */
    protected OnInterceptMoveEventListener mOnInterceptMoveEventListener;

    /**
     * Attaches the MenuDrawer to the Activity.
     *
     * @param activity The activity that the MenuDrawer will be attached to.
     * @return The created MenuDrawer instance.
     */
    public static MenuDrawer attach(Activity activity) {
        return attach(activity, MENU_DRAG_CONTENT);
    }

    /**
     * Attaches the MenuDrawer to the Activity.
     *
     * @param activity The activity the menu drawer will be attached to.
     * @param dragMode The drag mode of the drawer. Can be either {@link MenuDrawer#MENU_DRAG_CONTENT}
     *                 or {@link MenuDrawer#MENU_DRAG_WINDOW}.
     * @return The created MenuDrawer instance.
     */
    public static MenuDrawer attach(Activity activity, int dragMode) {
        return attach(activity, dragMode, Position.LEFT);
    }

    /**
     * Attaches the MenuDrawer to the Activity.
     *
     * @param activity The activity the menu drawer will be attached to.
     * @param position Where to position the menu.
     * @return The created MenuDrawer instance.
     */
    public static MenuDrawer attach(Activity activity, Position position) {
        return attach(activity, MENU_DRAG_CONTENT, position);
    }

    /**
     * Attaches the MenuDrawer to the Activity.
     *
     * @param activity The activity the menu drawer will be attached to.
     * @param dragMode The drag mode of the drawer. Can be either {@link MenuDrawer#MENU_DRAG_CONTENT}
     *                 or {@link MenuDrawer#MENU_DRAG_WINDOW}.
     * @param position Where to position the menu.
     * @return The created MenuDrawer instance.
     */
    public static MenuDrawer attach(Activity activity, int dragMode, Position position) {
        return attach(activity, dragMode, position, false);
    }

    /**
     * Attaches the MenuDrawer to the Activity.
     *
     * @param activity     The activity the menu drawer will be attached to.
     * @param dragMode     The drag mode of the drawer. Can be either {@link MenuDrawer#MENU_DRAG_CONTENT}
     *                     or {@link MenuDrawer#MENU_DRAG_WINDOW}.
     * @param position     Where to position the menu.
     * @param attachStatic Whether a static (non-draggable, always visible) drawer should be used.
     * @return The created MenuDrawer instance.
     */
    public static MenuDrawer attach(Activity activity, int dragMode, Position position, boolean attachStatic) {
        MenuDrawer menuDrawer = createMenuDrawer(activity, dragMode, position, attachStatic);
        menuDrawer.setId(R.id.md__drawer);

        switch (dragMode) {
            case MenuDrawer.MENU_DRAG_CONTENT:
                attachToContent(activity, menuDrawer);
                break;

            case MenuDrawer.MENU_DRAG_WINDOW:
                attachToDecor(activity, menuDrawer);
                break;

            default:
                throw new RuntimeException("Unknown menu mode: " + dragMode);
        }

        return menuDrawer;
    }

    /**
     * Constructs the appropriate MenuDrawer based on the position.
     */
    private static MenuDrawer createMenuDrawer(Activity activity, int dragMode, Position position,
            boolean attachStatic) {
        if (attachStatic) {
            switch (position) {
                case LEFT:
                    return new LeftStaticDrawer(activity, dragMode);
                case RIGHT:
                    return new RightStaticDrawer(activity, dragMode);
                case TOP:
                    return new TopStaticDrawer(activity, dragMode);
                case BOTTOM:
                    return new BottomStaticDrawer(activity, dragMode);
                default:
                    throw new IllegalArgumentException("position must be one of LEFT, TOP, RIGHT or BOTTOM");
            }
        }

        switch (position) {
            case LEFT:
                return new LeftDrawer(activity, dragMode);
            case RIGHT:
                return new RightDrawer(activity, dragMode);
            case TOP:
                return new TopDrawer(activity, dragMode);
            case BOTTOM:
                return new BottomDrawer(activity, dragMode);
            default:
                throw new IllegalArgumentException("position must be one of LEFT, TOP, RIGHT or BOTTOM");
        }
    }

    /**
     * Attaches the menu drawer to the content view.
     */
    private static void attachToContent(Activity activity, MenuDrawer menuDrawer) {
        /**
         * Do not call mActivity#setContentView.
         * E.g. if using with a ListActivity, Activity#setContentView is overridden and dispatched to
         * MenuDrawer#setContentView, which then again would call Activity#setContentView.
         */
        ViewGroup content = (ViewGroup) activity.findViewById(android.R.id.content);
        content.removeAllViews();
        content.addView(menuDrawer, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    }

    /**
     * Attaches the menu drawer to the window.
     */
    private static void attachToDecor(Activity activity, MenuDrawer menuDrawer) {
        ViewGroup decorView = (ViewGroup) activity.getWindow().getDecorView();
        ViewGroup decorChild = (ViewGroup) decorView.getChildAt(0);

        decorView.removeAllViews();
        decorView.addView(menuDrawer, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

        menuDrawer.mContentContainer.addView(decorChild, decorChild.getLayoutParams());
    }

    MenuDrawer(Activity activity, int dragMode) {
        this(activity);

        mActivity = activity;
        mDragMode = dragMode;
    }

    public MenuDrawer(Context context) {
        this(context, null);
    }

    public MenuDrawer(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuDrawerStyle);
    }

    public MenuDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        initDrawer(context, attrs, defStyle);
    }

    protected void initDrawer(Context context, AttributeSet attrs, int defStyle) {
        setWillNotDraw(false);
        setFocusable(false);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.MenuDrawer, R.attr.menuDrawerStyle,
                R.style.Widget_MenuDrawer);

        final Drawable contentBackground = a.getDrawable(R.styleable.MenuDrawer_mdContentBackground);
        final Drawable menuBackground = a.getDrawable(R.styleable.MenuDrawer_mdMenuBackground);

        mMenuSize = a.getDimensionPixelSize(R.styleable.MenuDrawer_mdMenuSize, -1);
        mMenuSizeSet = mMenuSize != -1;

        final int indicatorResId = a.getResourceId(R.styleable.MenuDrawer_mdActiveIndicator, 0);
        if (indicatorResId != 0) {
            mActiveIndicator = BitmapFactory.decodeResource(getResources(), indicatorResId);
        }

        mDropShadowEnabled = a.getBoolean(R.styleable.MenuDrawer_mdDropShadowEnabled, true);

        mDropShadowDrawable = a.getDrawable(R.styleable.MenuDrawer_mdDropShadow);

        if (mDropShadowDrawable == null) {
            final int dropShadowColor = a.getColor(R.styleable.MenuDrawer_mdDropShadowColor, 0xFF000000);
            setDropShadowColor(dropShadowColor);
        }

        mDropShadowSize = a.getDimensionPixelSize(R.styleable.MenuDrawer_mdDropShadowSize,
                dpToPx(DEFAULT_DROP_SHADOW_DP));

        mTouchBezelSize = a.getDimensionPixelSize(R.styleable.MenuDrawer_mdTouchBezelSize,
                dpToPx(DEFAULT_DRAG_BEZEL_DP));

        mAllowIndicatorAnimation = a.getBoolean(R.styleable.MenuDrawer_mdAllowIndicatorAnimation, false);

        mMaxAnimationDuration = a.getInt(R.styleable.MenuDrawer_mdMaxAnimationDuration, DEFAULT_ANIMATION_DURATION);

        a.recycle();

        mMenuContainer = new BuildLayerFrameLayout(context);
        mMenuContainer.setId(R.id.md__menu);
        mMenuContainer.setBackgroundDrawable(menuBackground);
        super.addView(mMenuContainer, -1, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        mContentContainer = new NoClickThroughFrameLayout(context);
        mContentContainer.setId(R.id.md__content);
        mContentContainer.setBackgroundDrawable(contentBackground);
        super.addView(mContentContainer, -1, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        mMenuOverlay = new ColorDrawable(0xFF000000);

        mIndicatorScroller = new FloatScroller(SMOOTH_INTERPOLATOR);
    }

    @Override
    public void addView(View child, int index, LayoutParams params) {
        int childCount = mMenuContainer.getChildCount();
        if (childCount == 0) {
            mMenuContainer.addView(child, index, params);
            return;
        }

        childCount = mContentContainer.getChildCount();
        if (childCount == 0) {
            mContentContainer.addView(child, index, params);
            return;
        }

        throw new IllegalStateException("MenuDrawer can only hold two child views");
    }

    protected int dpToPx(int dp) {
        return (int) (getResources().getDisplayMetrics().density * dp + 0.5f);
    }

    protected boolean isViewDescendant(View v) {
        ViewParent parent = v.getParent();
        while (parent != null) {
            if (parent == this) {
                return true;
            }

            parent = parent.getParent();
        }

        return false;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        getViewTreeObserver().addOnScrollChangedListener(mScrollListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        getViewTreeObserver().removeOnScrollChangedListener(mScrollListener);
        super.onDetachedFromWindow();
    }

    /**
     * Toggles the menu open and close with animation.
     */
    public void toggleMenu() {
        toggleMenu(true);
    }

    /**
     * Toggles the menu open and close.
     *
     * @param animate Whether open/close should be animated.
     */
    public abstract void toggleMenu(boolean animate);

    /**
     * Animates the menu open.
     */
    public void openMenu() {
        openMenu(true);
    }

    /**
     * Opens the menu.
     *
     * @param animate Whether open/close should be animated.
     */
    public abstract void openMenu(boolean animate);

    /**
     * Animates the menu closed.
     */
    public void closeMenu() {
        closeMenu(true);
    }

    /**
     * Closes the menu.
     *
     * @param animate Whether open/close should be animated.
     */
    public abstract void closeMenu(boolean animate);

    /**
     * Indicates whether the menu is currently visible.
     *
     * @return True if the menu is open, false otherwise.
     */
    public abstract boolean isMenuVisible();

    /**
     * Set the size of the menu drawer when open.
     *
     * @param size The size of the menu.
     */
    public abstract void setMenuSize(int size);

    /**
     * Returns the size of the menu.
     *
     * @return The size of the menu.
     */
    public int getMenuSize() {
        return mMenuSize;
    }

    /**
     * Set the active view.
     * If the mdActiveIndicator attribute is set, this View will have the indicator drawn next to it.
     *
     * @param v The active view.
     */
    public void setActiveView(View v) {
        setActiveView(v, 0);
    }

    /**
     * Set the active view.
     * If the mdActiveIndicator attribute is set, this View will have the indicator drawn next to it.
     *
     * @param v        The active view.
     * @param position Optional position, usually used with ListView. v.setTag(R.id.mdActiveViewPosition, position)
     *                 must be called first.
     */
    public void setActiveView(View v, int position) {
        final View oldView = mActiveView;
        mActiveView = v;
        mActivePosition = position;

        if (mAllowIndicatorAnimation && oldView != null) {
            startAnimatingIndicator();
        }

        invalidate();
    }

    /**
     * Sets whether the indicator should be animated between active views.
     *
     * @param animate Whether the indicator should be animated between active views.
     */
    public void setAllowIndicatorAnimation(boolean animate) {
        if (animate != mAllowIndicatorAnimation) {
            mAllowIndicatorAnimation = animate;
            completeAnimatingIndicator();
        }
    }

    /**
     * Indicates whether the indicator should be animated between active views.
     *
     * @return Whether the indicator should be animated between active views.
     */
    public boolean getAllowIndicatorAnimation() {
        return mAllowIndicatorAnimation;
    }

    /**
     * Scroll listener that checks whether the active view has moved before the drawer is invalidated.
     */
    private ViewTreeObserver.OnScrollChangedListener mScrollListener = new ViewTreeObserver.OnScrollChangedListener() {
        @Override
        public void onScrollChanged() {
            if (mActiveView != null && isViewDescendant(mActiveView)) {
                mActiveView.getDrawingRect(mTempRect);
                offsetDescendantRectToMyCoords(mActiveView, mTempRect);
                if (mTempRect.left != mActiveRect.left || mTempRect.top != mActiveRect.top
                        || mTempRect.right != mActiveRect.right || mTempRect.bottom != mActiveRect.bottom) {
                    invalidate();
                }
            }
        }
    };

    /**
     * Starts animating the indicator to a new position.
     */
    private void startAnimatingIndicator() {
        mIndicatorStartPos = getIndicatorStartPos();
        mIndicatorAnimating = true;
        mIndicatorScroller.startScroll(0.0f, 1.0f, INDICATOR_ANIM_DURATION);

        animateIndicatorInvalidate();
    }

    /**
     * Returns the start position of the indicator.
     *
     * @return The start position of the indicator.
     */
    protected abstract int getIndicatorStartPos();

    /**
     * Callback when each frame in the indicator animation should be drawn.
     */
    private void animateIndicatorInvalidate() {
        if (mIndicatorScroller.computeScrollOffset()) {
            mIndicatorOffset = mIndicatorScroller.getCurr();
            invalidate();

            if (!mIndicatorScroller.isFinished()) {
                postOnAnimation(mIndicatorRunnable);
                return;
            }
        }

        completeAnimatingIndicator();
    }

    /**
     * Called when the indicator animation has completed.
     */
    private void completeAnimatingIndicator() {
        mIndicatorOffset = 1.0f;
        mIndicatorAnimating = false;
        invalidate();
    }

    /**
     * Enables or disables offsetting the menu when dragging the drawer.
     *
     * @param offsetMenu True to offset the menu, false otherwise.
     */
    public abstract void setOffsetMenuEnabled(boolean offsetMenu);

    /**
     * Indicates whether the menu is being offset when dragging the drawer.
     *
     * @return True if the menu is being offset, false otherwise.
     */
    public abstract boolean getOffsetMenuEnabled();

    public int getDrawerState() {
        return mDrawerState;
    }

    /**
     * Register a callback to be invoked when the drawer state changes.
     *
     * @param listener The callback that will run.
     */
    public void setOnDrawerStateChangeListener(OnDrawerStateChangeListener listener) {
        mOnDrawerStateChangeListener = listener;
    }

    /**
     * Register a callback that will be invoked when the drawer is about to intercept touch events.
     *
     * @param listener The callback that will be invoked.
     */
    public void setOnInterceptMoveEventListener(OnInterceptMoveEventListener listener) {
        mOnInterceptMoveEventListener = listener;
    }

    /**
     * Defines whether the drop shadow is enabled.
     *
     * @param enabled Whether the drop shadow is enabled.
     */
    public void setDropShadowEnabled(boolean enabled) {
        mDropShadowEnabled = enabled;
        invalidate();
    }

    /**
     * Sets the color of the drop shadow.
     *
     * @param color The color of the drop shadow.
     */
    public abstract void setDropShadowColor(int color);

    /**
     * Sets the drawable of the drop shadow.
     *
     * @param drawable The drawable of the drop shadow.
     */
    public void setDropShadow(Drawable drawable) {
        mDropShadowDrawable = drawable;
        invalidate();
    }

    /**
     * Sets the drawable of the drop shadow.
     *
     * @param resId The resource identifier of the the drawable.
     */
    public void setDropShadow(int resId) {
        setDropShadow(getResources().getDrawable(resId));
    }

    /**
     * Returns the drawable of the drop shadow.
     */
    public Drawable getDropShadow() {
        return mDropShadowDrawable;
    }

    /**
     * Sets the size of the drop shadow.
     *
     * @param size The size of the drop shadow in px.
     */
    public void setDropShadowSize(int size) {
        mDropShadowSize = size;
        invalidate();
    }

    /**
     * Animates the drawer slightly open until the user opens the drawer.
     */
    public abstract void peekDrawer();

    /**
     * Animates the drawer slightly open. If delay is larger than 0, this happens until the user opens the drawer.
     *
     * @param delay The delay (in milliseconds) between each run of the animation. If 0, this animation is only run
     *              once.
     */
    public abstract void peekDrawer(long delay);

    /**
     * Animates the drawer slightly open. If delay is larger than 0, this happens until the user opens the drawer.
     *
     * @param startDelay The delay (in milliseconds) until the animation is first run.
     * @param delay      The delay (in milliseconds) between each run of the animation. If 0, this animation is only run
     *                   once.
     */
    public abstract void peekDrawer(long startDelay, long delay);

    /**
     * Enables or disables the user of {@link View#LAYER_TYPE_HARDWARE} when animations views.
     *
     * @param enabled Whether hardware layers are enabled.
     */
    public abstract void setHardwareLayerEnabled(boolean enabled);

    /**
     * Sets the maximum duration of open/close animations.
     * @param duration The maximum duration in milliseconds.
     */
    public void setMaxAnimationDuration(int duration) {
        mMaxAnimationDuration = duration;
    }

    /**
     * Returns the ViewGroup used as a parent for the menu view.
     *
     * @return The menu view's parent.
     */
    public ViewGroup getMenuContainer() {
        return mMenuContainer;
    }

    /**
     * Returns the ViewGroup used as a parent for the content view.
     *
     * @return The content view's parent.
     */
    public ViewGroup getContentContainer() {
        if (mDragMode == MENU_DRAG_CONTENT) {
            return mContentContainer;
        } else {
            return (ViewGroup) findViewById(android.R.id.content);
        }
    }

    /**
     * Set the menu view from a layout resource.
     *
     * @param layoutResId Resource ID to be inflated.
     */
    public void setMenuView(int layoutResId) {
        mMenuContainer.removeAllViews();
        mMenuView = LayoutInflater.from(getContext()).inflate(layoutResId, mMenuContainer, false);
        mMenuContainer.addView(mMenuView);
    }

    /**
     * Set the menu view to an explicit view.
     *
     * @param view The menu view.
     */
    public void setMenuView(View view) {
        setMenuView(view, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * Set the menu view to an explicit view.
     *
     * @param view   The menu view.
     * @param params Layout parameters for the view.
     */
    public void setMenuView(View view, LayoutParams params) {
        mMenuView = view;
        mMenuContainer.removeAllViews();
        mMenuContainer.addView(view, params);
    }

    /**
     * Returns the menu view.
     *
     * @return The menu view.
     */
    public View getMenuView() {
        return mMenuView;
    }

    /**
     * Set the content from a layout resource.
     *
     * @param layoutResId Resource ID to be inflated.
     */
    public void setContentView(int layoutResId) {
        switch (mDragMode) {
            case MenuDrawer.MENU_DRAG_CONTENT:
                mContentContainer.removeAllViews();
                LayoutInflater.from(getContext()).inflate(layoutResId, mContentContainer, true);
                break;

            case MenuDrawer.MENU_DRAG_WINDOW:
                mActivity.setContentView(layoutResId);
                break;
        }
    }

    /**
     * Set the content to an explicit view.
     *
     * @param view The desired content to display.
     */
    public void setContentView(View view) {
        setContentView(view, new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * Set the content to an explicit view.
     *
     * @param view   The desired content to display.
     * @param params Layout parameters for the view.
     */
    public void setContentView(View view, LayoutParams params) {
        switch (mDragMode) {
            case MenuDrawer.MENU_DRAG_CONTENT:
                mContentContainer.removeAllViews();
                mContentContainer.addView(view, params);
                break;

            case MenuDrawer.MENU_DRAG_WINDOW:
                mActivity.setContentView(view, params);
                break;
        }
    }

    protected void setDrawerState(int state) {
        if (state != mDrawerState) {
            final int oldState = mDrawerState;
            mDrawerState = state;
            if (mOnDrawerStateChangeListener != null) mOnDrawerStateChangeListener.onDrawerStateChange(oldState, state);
            if (DEBUG) logDrawerState(state);
        }
    }

    protected void logDrawerState(int state) {
        switch (state) {
            case STATE_CLOSED:
                Log.d(TAG, "[DrawerState] STATE_CLOSED");
                break;

            case STATE_CLOSING:
                Log.d(TAG, "[DrawerState] STATE_CLOSING");
                break;

            case STATE_DRAGGING:
                Log.d(TAG, "[DrawerState] STATE_DRAGGING");
                break;

            case STATE_OPENING:
                Log.d(TAG, "[DrawerState] STATE_OPENING");
                break;

            case STATE_OPEN:
                Log.d(TAG, "[DrawerState] STATE_OPEN");
                break;

            default:
                Log.d(TAG, "[DrawerState] Unknown: " + state);
        }
    }

    /**
     * Returns the touch mode.
     */
    public abstract int getTouchMode();

    /**
     * Sets the drawer touch mode. Possible values are {@link #TOUCH_MODE_NONE}, {@link #TOUCH_MODE_BEZEL} or
     * {@link #TOUCH_MODE_FULLSCREEN}.
     *
     * @param mode The touch mode.
     */
    public abstract void setTouchMode(int mode);

    /**
     * Sets the size of the touch bezel.
     *
     * @param size The touch bezel size in px.
     */
    public abstract void setTouchBezelSize(int size);

    /**
     * Returns the size of the touch bezel in px.
     */
    public abstract int getTouchBezelSize();

    @Override
    public void postOnAnimation(Runnable action) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            super.postOnAnimation(action);
        } else {
            postDelayed(action, ANIMATION_DELAY);
        }
    }

    @Override
    protected boolean fitSystemWindows(Rect insets) {
        if (mDragMode == MENU_DRAG_WINDOW) {
            mMenuContainer.setPadding(0, insets.top, 0, 0);
        }
        return super.fitSystemWindows(insets);
    }

    /**
     * Saves the state of the drawer.
     *
     * @return Returns a Parcelable containing the drawer state.
     */
    public final Parcelable saveState() {
        if (mState == null) mState = new Bundle();
        saveState(mState);
        return mState;
    }

    void saveState(Bundle state) {
        // State saving isn't required for subclasses.
    }

    /**
     * Restores the state of the drawer.
     *
     * @param in A parcelable containing the drawer state.
     */
    public void restoreState(Parcelable in) {
        mState = (Bundle) in;
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        SavedState state = new SavedState(superState);

        if (mState == null) mState = new Bundle();
        saveState(mState);

        state.mState = mState;
        return state;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        SavedState savedState = (SavedState) state;
        super.onRestoreInstanceState(savedState.getSuperState());

        restoreState(savedState.mState);
    }

    static class SavedState extends BaseSavedState {

        Bundle mState;

        public SavedState(Parcelable superState) {
            super(superState);
        }

        public SavedState(Parcel in) {
            super(in);
            mState = in.readBundle();
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            super.writeToParcel(dest, flags);
            dest.writeBundle(mState);
        }

        @SuppressWarnings("UnusedDeclaration")
        public static final Creator<SavedState> CREATOR = new Creator<SavedState>() {
            @Override
            public SavedState createFromParcel(Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(int size) {
                return new SavedState[size];
            }
        };
    }
}
