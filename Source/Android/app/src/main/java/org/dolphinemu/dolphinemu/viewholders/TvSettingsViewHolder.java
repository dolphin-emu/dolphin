package org.dolphinemu.dolphinemu.viewholders;

import android.support.v17.leanback.widget.ImageCardView;
import android.support.v17.leanback.widget.Presenter;
import android.view.View;

public final class TvSettingsViewHolder extends Presenter.ViewHolder
{
	public ImageCardView cardParent;

	// Determines what action to take when this item is clicked.
	public int itemId;

	public TvSettingsViewHolder(View itemView)
	{
		super(itemView);

		itemView.setTag(this);

		cardParent = (ImageCardView) itemView;
	}
}
