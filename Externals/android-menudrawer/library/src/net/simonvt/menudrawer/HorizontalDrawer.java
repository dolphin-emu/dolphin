package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.VelocityTracker;

public abstract class HorizontalDrawer extends DraggableDrawer {

    private static final String TAG = "HorizontalDrawer";

    HorizontalDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public HorizontalDrawer(Context context) {
        super(context);
    }

    public HorizontalDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public HorizontalDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        final int heightMode = MeasureSpec.getMode(heightMeasureSpec);

        if (widthMode != MeasureSpec.EXACTLY || heightMode != MeasureSpec.EXACTLY) {
            throw new IllegalStateException("Must measure with an exact size");
        }

        final int width = MeasureSpec.getSize(widthMeasureSpec);
        final int height = MeasureSpec.getSize(heightMeasureSpec);

        if (!mMenuSizeSet) mMenuSize = (int) (width * 0.8f);
        if (mOffsetPixels == -1) openMenu(false);

        final int menuWidthMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, mMenuSize);
        final int menuHeightMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, height);
        mMenuContainer.measure(menuWidthMeasureSpec, menuHeightMeasureSpec);

        final int contentWidthMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, width);
        final int contentHeightMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, height);
        mContentContainer.measure(contentWidthMeasureSpec, contentHeightMeasureSpec);

        setMeasuredDimension(width, height);

        updateTouchAreaSize();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        final int action = ev.getAction() & MotionEvent.ACTION_MASK;

        if (action == MotionEvent.ACTION_DOWN && mMenuVisible && isCloseEnough()) {
            setOffsetPixels(0);
            stopAnimation();
            endPeek();
            setDrawerState(STATE_CLOSED);
        }

        // Always intercept events over the content while menu is visible.
        if (mMenuVisible && isContentTouch(ev)) return true;

        if (mTouchMode == TOUCH_MODE_NONE) {
            return false;
        }

        if (action != MotionEvent.ACTION_DOWN) {
            if (mIsDragging) return true;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN: {
                mLastMotionX = mInitialMotionX = ev.getX();
                mLastMotionY = mInitialMotionY = ev.getY();
                final boolean allowDrag = onDownAllowDrag(ev);

                if (allowDrag) {
                    setDrawerState(mMenuVisible ? STATE_OPEN : STATE_CLOSED);
                    stopAnimation();
                    endPeek();
                    mIsDragging = false;
                }
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                final float x = ev.getX();
                final float dx = x - mLastMotionX;
                final float xDiff = Math.abs(dx);
                final float y = ev.getY();
                final float yDiff = Math.abs(y - mLastMotionY);

                if (xDiff > mTouchSlop && xDiff > yDiff) {
                    if (mOnInterceptMoveEventListener != null && mTouchMode == TOUCH_MODE_FULLSCREEN
                            && canChildScrollHorizontally(mContentContainer, false, (int) dx, (int) x, (int) y)) {
                        endDrag(); // Release the velocity tracker
                        return false;
                    }

                    final boolean allowDrag = onMoveAllowDrag(ev, dx);

                    if (allowDrag) {
                        setDrawerState(STATE_DRAGGING);
                        mIsDragging = true;
                        mLastMotionX = x;
                        mLastMotionY = y;
                    }
                }
                break;
            }

            /**
             * If you click really fast, an up or cancel event is delivered here.
             * Just snap content to whatever is closest.
             * */
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP: {
                if (Math.abs(mOffsetPixels) > mMenuSize / 2) {
                    openMenu();
                } else {
                    closeMenu();
                }
                break;
            }
        }

        if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
        mVelocityTracker.addMovement(ev);

        return mIsDragging;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (!mMenuVisible && !mIsDragging && (mTouchMode == TOUCH_MODE_NONE)) {
            return false;
        }
        final int action = ev.getAction() & MotionEvent.ACTION_MASK;

        if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
        mVelocityTracker.addMovement(ev);

        switch (action) {
            case MotionEvent.ACTION_DOWN: {
                mLastMotionX = mInitialMotionX = ev.getX();
                mLastMotionY = mInitialMotionY = ev.getY();
                final boolean allowDrag = onDownAllowDrag(ev);

                if (allowDrag) {
                    stopAnimation();
                    endPeek();
                    startLayerTranslation();
                }
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                if (!mIsDragging) {
                    final float x = ev.getX();
                    final float dx = x - mLastMotionX;
                    final float xDiff = Math.abs(dx);
                    final float y = ev.getY();
                    final float yDiff = Math.abs(y - mLastMotionY);

                    if (xDiff > mTouchSlop && xDiff > yDiff) {
                        final boolean allowDrag = onMoveAllowDrag(ev, dx);

                        if (allowDrag) {
                            setDrawerState(STATE_DRAGGING);
                            mIsDragging = true;
                            mLastMotionX = x - mInitialMotionX > 0
                                    ? mInitialMotionX + mTouchSlop
                                    : mInitialMotionX - mTouchSlop;
                        }
                    }
                }

                if (mIsDragging) {
                    startLayerTranslation();

                    final float x = ev.getX();
                    final float dx = x - mLastMotionX;

                    mLastMotionX = x;
                    onMoveEvent(dx);
                }
                break;
            }

            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP: {
                onUpEvent(ev);
                break;
            }
        }

        return true;
    }
}
