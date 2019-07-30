package org.dolphinemu.dolphinemu.activities;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.text.Editable;
import android.text.Spanned;
import android.text.TextWatcher;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.StringReader;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public class EditorActivity extends AppCompatActivity
{
  private static class CheatEntry
  {
    public static final int TYPE_NONE = -1;
    public static final int TYPE_AR = 0;
    public static final int TYPE_GECKO = 1;

    public String name = "";
    public String info = "";
    public int type = TYPE_NONE;
    public boolean active = false;
  }

  private static class IniTextWatcher implements TextWatcher
  {
    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after)
    {

    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count)
    {

    }

    @Override
    public void afterTextChanged(Editable s)
    {
      int offset = 0;
      int sepsize = System.lineSeparator().length();
      BufferedReader reader = new BufferedReader(new StringReader(s.toString()));
      try
      {
        String line = reader.readLine();
        while (line != null)
        {
          int len = line.length();
          boolean isFirstChar = true;
          int commentStart = -1;
          int codenameStart = -1;
          int sectionStart = -1;
          int sectionEnd = -1;
          for (int i = 0; i < len; ++i)
          {
            switch (line.charAt(i))
            {
              case '\t':
              case ' ':
                break;
              case '#':
                if (commentStart == -1)
                  commentStart = i;
                isFirstChar = false;
                break;
              case '$':
                codenameStart = isFirstChar ? i : -1;
                isFirstChar = false;
              case '[':
                sectionStart = isFirstChar ? i : -1;
                isFirstChar = false;
                break;
              case ']':
                if(sectionStart != -1 && sectionEnd == -1)
                {
                  sectionEnd = i;
                }
                else
                {
                  sectionStart = -1;
                  sectionEnd = -1;
                }
                isFirstChar = false;
                break;
              default:
                isFirstChar = false;
                break;
            }
          }

          if (commentStart != -1)
          {
            s.setSpan(new ForegroundColorSpan(Color.GRAY), offset + commentStart,
              offset + len, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
          }
          else if (codenameStart != -1)
          {
            s.setSpan(new ForegroundColorSpan(Color.MAGENTA), offset + codenameStart,
              offset + len, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
          }
          else if (sectionStart != -1 && sectionEnd != -1)
          {
            s.setSpan(new ForegroundColorSpan(Color.BLUE), offset + sectionStart,
              offset + sectionEnd + 1, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
          }

          offset += len + sepsize;
          line = reader.readLine();
        }
      }
      catch (IOException e)
      {
        // ignore
      }
    }
  }

  private static class DownloaderTask extends AsyncTask<String, String, String>
  {
    private int mSelection;
    private String mContent;
    private WeakReference<EditorActivity> mActivity;

    public DownloaderTask(EditorActivity editor)
    {
      mActivity = new WeakReference<>(editor);
    }

    @Override
    protected void onPreExecute()
    {
      mContent = mActivity.get().mEditor.getText().toString();
      mActivity.get().mProgressBar.setVisibility(View.VISIBLE);
      mSelection = mActivity.get().mEditor.getSelectionStart();
    }

    @Override
    protected String doInBackground(String... gametdbId)
    {
      OkHttpClient downloader = new OkHttpClient();
      Response response = null;
      Request request = new Request.Builder()
        .url("https://www.geckocodes.org/txt.php?txt=" + gametdbId[0])
        .addHeader("Cookie", "challenge=BitMitigate.com")
        .build();

      String result = "";
      try
      {
        response = downloader.newCall(request).execute();
        if (response != null && response.code() == 200 && response.body() != null)
        {
          result = response.body().string();
        }
      }
      catch (IOException e)
      {
        // ignore
      }
      return processGeckoCodes(result);
    }

    @Override
    protected void onPostExecute(String param)
    {
      if (mActivity.get() != null)
      {
        mActivity.get().mEditor.setText(param);
        mActivity.get().mEditor.setSelection(mSelection);
        mActivity.get().mProgressBar.setVisibility(View.INVISIBLE);
        mActivity.get().loadCheatList();
      }
    }

    private String processGeckoCodes(String codes)
    {
      if(codes.isEmpty() || codes.charAt(0) == '<')
        return mContent;

      int state = 0;
      StringBuilder codeSB = new StringBuilder();
      BufferedReader reader = new BufferedReader(new StringReader(codes));
      try
      {
        reader.readLine();
        reader.readLine();
        reader.readLine();
        //
        String line = reader.readLine();
        while (line != null)
        {
          if (state == 0)
          {
            // try read name
            if (!line.isEmpty())
            {
              if (line.charAt(0) == '*')
              {
                // read comments
                codeSB.append(line);
                codeSB.append('\n');
              }
              else
              {
                // read name
                int pos = line.lastIndexOf(' ');
                if (pos != -1 && line.length() > pos + 2 && line.charAt(pos + 1) == '[')
                {
                  line = line.substring(0, pos);
                }
                codeSB.append('$');
                codeSB.append(line);
                codeSB.append('\n');
                state = 1;
              }
            }
          }
          else if (state == 1)
          {
            // read codes
            if (line.length() == 17)
            {
              String[] codePart = line.split(" ");
              if (codePart.length != 2 || codePart[0].length() != 8 || codePart[1].length() != 8)
              {
                codeSB.append('*');
                state = 2;
              }
              codeSB.append(line);
              codeSB.append('\n');
            }
            else
            {
              state = line.isEmpty() ? 0 : 2;
            }
          }
          else if (state == 2)
          {
            // read comments
            if (!line.isEmpty())
            {
              codeSB.append('*');
              codeSB.append(line);
              codeSB.append('\n');
            }
            else
            {
              // goto read name
              state = 0;
            }
          }
          line = reader.readLine();
        }
      }
      catch (IOException e)
      {
        // ignore
      }

      boolean isInsert = false;
      StringBuilder configSB = new StringBuilder();
      reader = new BufferedReader(new StringReader(mContent));
      try
      {
        String line = reader.readLine();
        while (line != null)
        {
          configSB.append(line);
          configSB.append('\n');
          if (!isInsert && line.contains("[Gecko]"))
          {
            mSelection = configSB.length();
            configSB.append(codeSB.toString());
            isInsert = true;
          }
          line = reader.readLine();
        }
      }
      catch (IOException e)
      {
        // ignore
      }

      if(!isInsert)
      {
        configSB.append("\n[Gecko]\n");
        mSelection = configSB.length();
        configSB.append(codeSB.toString());
      }

      return configSB.toString();
    }
  }

  class CheatEntryViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
  {
    private CheatEntry mModel;
    private TextView mTextName;
    private TextView mTextDescription;
    private CheckBox mCheckbox;

    public CheatEntryViewHolder(View itemView)
    {
      super(itemView);
      mTextName = itemView.findViewById(R.id.text_setting_name);
      mTextDescription = itemView.findViewById(R.id.text_setting_description);
      mCheckbox = itemView.findViewById(R.id.checkbox);
      itemView.setOnClickListener(this);
    }

    public void bind(CheatEntry entry)
    {
      mModel = entry;
      mTextName.setText(entry.name);
      mTextDescription.setText(entry.info);
      mCheckbox.setChecked(entry.active);
    }

    @Override
    public void onClick(View v)
    {
      toggleCheatEntry(mModel);
      mCheckbox.setChecked(mModel.active);
    }
  }

  class CheatEntryAdapter extends RecyclerView.Adapter<CheatEntryViewHolder>
  {
    private List<CheatEntry> mDataset;

    @NonNull
    @Override
    public CheatEntryViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
    {
      LayoutInflater inflater = LayoutInflater.from(parent.getContext());
      View itemView = inflater.inflate(R.layout.list_item_setting_checkbox, parent, false);
      return new CheatEntryViewHolder(itemView);
    }

    @Override
    public int getItemCount()
    {
      return mDataset != null ? mDataset.size() : 0;
    }

    @Override
    public void onBindViewHolder(@NonNull CheatEntryViewHolder holder, int position)
    {
      holder.bind(mDataset.get(position));
    }

    public void loadCodes(List<CheatEntry> list)
    {
      mDataset = list;
      notifyDataSetChanged();
    }
  }

  private static final String ARG_GAME_PATH = "game_path";

  public static void launch(Context context, String gamePath)
  {
    Intent settings = new Intent(context, EditorActivity.class);
    settings.putExtra(ARG_GAME_PATH, gamePath);
    context.startActivity(settings);
  }

  private final int SECTION_NONE = -1;
  private final int SECTION_AR = 0;
  private final int SECTION_AR_ENABLED = 1;
  private final int SECTION_GECKO = 2;
  private final int SECTION_GECKO_ENABLED = 3;
  private final String[] SECTIONS = {"[ActionReplay]", "[ActionReplay_Enabled]", "[Gecko]", "[Gecko_Enabled]"};

  private GameFile mGameFile;
  private EditText mEditor;
  private RecyclerView mListView;
  private CheatEntryAdapter mAdapter;
  private ProgressBar mProgressBar;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_editor);
    Toolbar toolbar = findViewById(R.id.toolbar);
    setSupportActionBar(toolbar);

    final String gamePath = getIntent().getStringExtra(ARG_GAME_PATH);
    final GameFile gameFile = GameFileCacheService.addOrGet(gamePath);
    final String gameId = gameFile.getGameId();
    setTitle(gameId);

    // code list
    Drawable lineDivider = getDrawable(R.drawable.line_divider);
    mListView = findViewById(R.id.code_list);
    mAdapter = new CheatEntryAdapter();
    mListView.setAdapter(mAdapter);
    mListView.addItemDecoration(new DividerItemDecoration(lineDivider));
    mListView.setLayoutManager(new LinearLayoutManager(this));

    // code editor
    mEditor = findViewById(R.id.code_content);

    mGameFile = gameFile;
    mProgressBar = findViewById(R.id.progress_bar);
    mProgressBar.setVisibility(View.INVISIBLE);

    mEditor.addTextChangedListener(new IniTextWatcher());
    mEditor.setHorizontallyScrolling(true);
    setGameSettings(gameId, mEditor);
    loadCheatList();

    // show
    toggleListView(true);

    Button buttonConfirm = findViewById(R.id.button_confirm);
    buttonConfirm.setOnClickListener(view ->
    {
      AcceptCheatCode(gameId, mEditor);
      mEditor.clearFocus();
      finish();
    });

    Button buttonCancel = findViewById(R.id.button_cancel);
    buttonCancel.setOnClickListener(view ->
    {
      mEditor.clearFocus();
      finish();
    });
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_editor, menu);
    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    switch (item.getItemId())
    {
      case R.id.menu_download_gecko:
        DownloadGeckoCodes(mGameFile.getGameTdbId(), mEditor);
        return true;
      case R.id.menu_toggle_list:
        toggleListView(mEditor.getVisibility() == View.VISIBLE);
        return true;
    }
    return false;
  }

  private void toggleListView(boolean isShowList)
  {
    if (isShowList)
    {
      InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
      imm.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
      mListView.setVisibility(View.VISIBLE);
      mEditor.setVisibility(View.INVISIBLE);
      loadCheatList();
    }
    else
    {
      mListView.setVisibility(View.INVISIBLE);
      mEditor.setVisibility(View.VISIBLE);
    }
  }

  private void toggleCheatEntry(CheatEntry entry)
  {
    final String section = entry.type == CheatEntry.TYPE_AR ? "[ActionReplay_Enabled]" : "[Gecko_Enabled]";
    int k = -1;
    Editable content = mEditor.getText();

    if (entry.active)
    {
      // remove enabled cheat code
      boolean isEnabledSection = false;
      String target = section;
      for (int i = 0; i < content.length(); ++i)
      {
        char c = content.charAt(i);
        if (c == '[')
        {
          // section begin
          isEnabledSection = false;
          k = i;
        }
        else if (c == '$')
        {
          if (isEnabledSection)
          {
            // cheat name begin
            k = i;
          }
          else
          {
            k = -1;
          }
        }
        else if (k != -1)
        {
          if (target.length() > i - k)
          {
            // check equals
            if (target.charAt(i - k) != c)
            {
              k = -1;
            }
          }
          else if (c == '\n')
          {
            if (isEnabledSection)
            {
              // delete cheat name
              content.delete(k, i + 1);
              break;
            }
            else
            {
              // search cheat name
              isEnabledSection = true;
              target = entry.name;
              k = -1;
            }
          }
        }
      }
    }
    else
    {
      // enable cheat code
      boolean insert = false;
      for (int i = 0; i < content.length(); ++i)
      {
        char c = content.charAt(i);
        if (c == '[')
        {
          // section begin
          k = i;
        }
        else if (k != -1)
        {
          if (section.length() > i - k)
          {
            // check equals
            if (section.charAt(i - k) != c)
            {
              k = -1;
            }
          }
          else if (c == '\n')
          {
            // insert cheat name
            content.insert(i + 1, entry.name + "\n");
            insert = true;
            break;
          }
        }
      }

      if (!insert)
      {
        if (k == -1)
        {
          if (content.charAt(content.length() - 1) != '\n')
          {
            content.append("\n");
          }
          content.append(section);
        }
        content.append("\n");
        content.append(entry.name);
        content.append("\n");
      }
    }

    entry.active = !entry.active;
  }

  private void loadCheatList()
  {
    int section = SECTION_NONE;
    CheatEntry entry = new CheatEntry();
    String[] lines = mEditor.getText().toString().split("\n");
    List<CheatEntry> list = new ArrayList<>();
    for (String line : lines)
    {
      if (line.isEmpty())
      {
        continue;
      }

      switch (line.charAt(0))
      {
        case '*':
          if(!entry.name.isEmpty())
          {
            // cheat description
            entry.info += line;
          }
          break;
        case '$':
          // new cheat entry begin
          if (entry.type != CheatEntry.TYPE_NONE)
          {
            addCheatEntry(list, entry);
            entry = new CheatEntry();
          }
          entry.name = line;
          switch (section)
          {
            case SECTION_AR:
              entry.type = CheatEntry.TYPE_AR;
              entry.active = false;
              break;
            case SECTION_AR_ENABLED:
              entry.type = CheatEntry.TYPE_AR;
              entry.active = true;
              break;
            case SECTION_GECKO:
              entry.type = CheatEntry.TYPE_GECKO;
              entry.active = false;
              break;
            case SECTION_GECKO_ENABLED:
              entry.type = CheatEntry.TYPE_GECKO;
              entry.active = true;
              break;
            case SECTION_NONE:
          }
          break;
        case '[':
          // new section begin
          if (section != SECTION_NONE)
          {
            if (entry.type != CheatEntry.TYPE_NONE)
            {
              addCheatEntry(list, entry);
              entry = new CheatEntry();
            }
            section = SECTION_NONE;
          }
          // is cheat section?
          for (int i = 0; i < SECTIONS.length; ++i)
          {
            int index = line.indexOf(SECTIONS[i]);
            if (index != -1)
            {
              section = i;
            }
          }
          break;
      }
    }

    // last one
    if (entry.type != CheatEntry.TYPE_NONE)
    {
      addCheatEntry(list, entry);
    }

    mAdapter.loadCodes(list);
  }

  private void addCheatEntry(List<CheatEntry> list, CheatEntry entry)
  {
    boolean isSaved = false;
    for (int i = 0; i < list.size(); ++i)
    {
      CheatEntry code = list.get(i);
      if (code.name.equals(entry.name))
      {
        code.active = entry.active | code.active;
        if (!entry.info.isEmpty())
        {
          code.info = entry.info;
        }
        isSaved = true;
      }
    }

    if (!isSaved)
    {
      list.add(entry);
    }
  }

  private BufferedReader getConfigReader(String gameId)
  {
    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
    File configFile = new File(filename);
    BufferedReader reader = null;

    if (configFile.exists())
    {
      try
      {
        reader = new BufferedReader(new FileReader(configFile));
      }
      catch (IOException e)
      {
        reader = null;
      }
    }
    else if (gameId.length() > 3)
    {
      try
      {
        String path = "Sys/GameSettings/" + gameId + ".ini";
        reader = new BufferedReader(new InputStreamReader(getAssets().open(path)));
      }
      catch (IOException e)
      {
        reader = null;
      }

      if (reader == null)
      {
        try
        {
          String path = "Sys/GameSettings/" + gameId.substring(0, 3) + ".ini";
          reader = new BufferedReader(new InputStreamReader(getAssets().open(path)));
        }
        catch (IOException e)
        {
          reader = null;
        }
      }
    }

    return reader;
  }

  private void setGameSettings(String gameId, EditText editCode)
  {
    BufferedReader reader = getConfigReader(gameId);
    if (reader == null)
    {
      return;
    }

    int count = 0;
    int[] indices = {-1, -1, -1, -1};
    StringBuilder sb = new StringBuilder();
    try
    {
      String line = reader.readLine();
      while (line != null)
      {
        line = trimString(line);
        if (line.length() > 0 && line.charAt(0) == '[')
        {
          // is cheat section?
          for (int i = 0; i < SECTIONS.length; ++i)
          {
            int index = line.indexOf(SECTIONS[i]);
            if (index != -1)
            {
              indices[i] = count + index;
            }
          }
        }
        //
        count += line.length() + 1;
        sb.append(line);
        sb.append(System.lineSeparator());
        line = reader.readLine();
      }
    }
    catch (Exception e)
    {
      // ignore
    }

    int cursorPos = 0;
    for (int i = 0; i < SECTIONS.length; ++i)
    {
      if (indices[i] < 0)
      {
        sb.append(System.lineSeparator());
        sb.append(SECTIONS[i]);
        sb.append(System.lineSeparator());
        count += SECTIONS[i].length() + 2;
        cursorPos = count;
      }
      else if (indices[i] > cursorPos)
      {
        cursorPos = indices[i] + SECTIONS[i].length() + 1;
      }
    }

    String content = sb.toString();
    if (cursorPos > content.length())
      cursorPos = content.length();
    editCode.setText(content);
    editCode.setSelection(cursorPos);
  }

  private void DownloadGeckoCodes(String gametdbId, EditText editCode)
  {
    new DownloaderTask(this).execute(gametdbId);
  }

  private String trimString(String text)
  {
    int len = text.length();
    int st = 0;

    while ((st < len) && (text.charAt(st) <= ' ' || text.charAt(st) == 160))
    {
      st++;
    }

    while ((st < len) && (text.charAt(len - 1) <= ' ' || text.charAt(len - 1) == 160))
    {
      len--;
    }

    return ((st > 0) || (len < text.length())) ? text.substring(st, len) : text;
  }

  private void AcceptCheatCode(String gameId, EditText editCode)
  {
    StringBuilder configSB = new StringBuilder();
    String content = editCode.getText().toString();
    BufferedReader reader = new BufferedReader(new StringReader(content));
    try
    {
      String line = reader.readLine();
      StringBuilder codeSB = new StringBuilder();
      while (line != null)
      {
        line = trimString(line);
        // encrypted ar code: P28E-EJY7-26PM5
        String[] blocks = line.split("-");
        if (blocks.length == 3 && blocks[0].length() == 4 && blocks[1].length() == 4 &&
          blocks[2].length() == 5)
        {
          codeSB.append(blocks[0]);
          codeSB.append(blocks[1]);
          codeSB.append(blocks[2]);
          codeSB.append('\n');
        }
        else
        {
          if (codeSB.length() > 0)
          {
            String encrypted = codeSB.toString();
            String arcode = NativeLibrary.DecryptARCode(encrypted);
            if (arcode.length() > 0)
            {
              configSB.append(arcode);
            }
            else
            {
              configSB.append(encrypted);
            }
            codeSB = new StringBuilder();
          }
          configSB.append(line);
          configSB.append('\n');
        }

        line = reader.readLine();
      }
      if (codeSB.length() > 0)
      {
        configSB.append(NativeLibrary.DecryptARCode(codeSB.toString()));
      }
    }
    catch (IOException e)
    {
      configSB = new StringBuilder();
      configSB.append(content);
    }

    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
    boolean saved = false;
    try
    {
      BufferedWriter writer = new BufferedWriter(new FileWriter(filename));
      writer.write(configSB.toString());
      writer.close();
      saved = true;
    }
    catch (Exception e)
    {
      // ignore
    }
    Toast.makeText(this, saved ? R.string.toast_save_code_ok : R.string.toast_save_code_no,
      Toast.LENGTH_SHORT).show();
  }
}
