//  This file is part of Empirical, https://github.com/devosoft/Empirical
//  Copyright (C) Michigan State University, 2015-2017.
//  Released under the MIT Software license; see doc/LICENSE
//
//
//  The Table widget
//
//  TableInfo is the core information for a table and has two helper classes:
//  TableRow and TableData.  The Table class is a smart pointer to a TableInfo
//  object.
//
//  A Table is composed of a series of rows, each with the same number of columns.
//  TableData may be muliple cells wide/tall, masking other cells.
//
//  Constructors:
//    Table(size_t r, size_t c, const std::string & in_id="")
//      Create a new r-by-c table with an optional DOM id specified.
//      State is initialized to TABLE; the first row and first cell are default locations.
//    Table(const Widget & in)
//      Point to an existing table (assert that widget IS a table!)
//
//  Accessors:
//    size_t GetNumCols() const
//    size_t GetNumRows() const
//    size_t GetNumCells() const
//      Return associated information about the table size.
//
//    size_t GetCurRow() const { return cur_row; }
//    size_t GetCurCol() const { return cur_col; }
//      Return information about the focal position on the table.
//
//  Adjusting Table size:
//    Table & Rows(size_t r)
//    Table & Cols(size_t c)
//    Table & Resize(size_t r, size_t c)
//      Set the number of rows, columns, or both in the table.
//
//  Setting or identifying the current table state:
//    bool InStateTable() const
//    bool InStateRow() const
//    bool InStateCell() const
//      Return true/false to identify what state the table is currently in.
//
//  Get table widget that affect specified cell, row, etc.
//    Table GetCell(size_t r, size_t c)
//    Table GetRow(size_t r)
//    Table GetCol(size_t c)
//    Table GetRowGroup(size_t r)
//    Table GetColGroup(size_t c)
//    Table GetTable()
//
//  Make subsequent calls to *this* widget affect specified cell, row, etc.
//    Table & SetCellActive(size_t r, size_t c)
//    Table & SetRowActive(size_t r)
//    Table & SetColActive(size_t c)
//    Table & SetRowGroupActive(size_t r)
//    Table & SetColGroupActive(size_t c)
//    Table & SetTableActive()
//
//  Modifying data in table
//    Table & SetHeader(bool _h=true)
//      Set the current cell to be a header (or not if false is passed in)
//    Widget AddText(size_t r, size_t c, const std::string & text)
//      Add text to the specified table cell.
//    Widget AddHeader(size_t r, size_t c, const std::string & text)
//      Add text to the specified table cell AND set the cell to be a header.
//    Table & SetRowSpan(size_t row_span)
//    Table & SetColSpan(size_t col_span)
//    Table & SetSpan(size_t row_span, size_t col_span)
//      Allow the row and/or column span of the current cell to be adjusted.
//    Table & SetSpan(size_t new_span)
//      Set the span of a row group or column group to the value provided.
//
//  Clearing table contents:
//    Table & ClearTable()
//      Clear all style information from table, remove contents from all cells, and
//      shrink table to no rows and no cells.
//    Table & ClearRows()
//      Clear style information from rows and cells and remove contents from cells
//      (but leave table style information and size.)
//    Table & ClearRow(size_t r)
//      Clear style information from the specified row and contents from all cells
//      in that row (but leave other rows untouched).
//    Table & ClearCells()
//      If state is TABLE, clear contents from all cells in entire table.
//      If state is ROW or COL, clear contents from all cells in that row/column.
//      If state is ROW_GROUP or COL_GROUP, clear contents of cells in all rows/cols in group
//      If state is CELL, clear just that single cell.
//    Table & ClearCell(size_t r, size_t c)
//      Clear contents of just the specified cell.
//    Table & Clear()
//      Dynamically clear the entire active state
//      (TABLE, ROW, COL, ROW_GROUP, COL_GROUP, or CELL).
//
//  Style manipulation
//    std::string GetCSS(const std::string & setting)
//    std::string GetCSS(const std::string & setting, SETTING_TYPE && value)
//      Get or Set the current value of the specified Style setting, based on the state of
//      the table (i.e., TABLE affects full table style, ROW affects active row style, and
//      CELL affects active cell style.)
//    Table & RowCSS(size_t row_id, const std::string & setting, SETTING_TYPE && value)
//    Table & CellCSS(size_t row_id, size_t col_id, const std::string & setting, SETTING_TYPE && value)
//      Set the specified row or cell Style to the value indicated.
//    Table & RowsCSS(const std::string & setting, SETTING_TYPE && value)
//    Table & CellsCSS(const std::string & setting, SETTING_TYPE && value)
//      Set the specified Style setting of all rows or all cells to the value indicated.
//
//  Developer notes:
//  * Tables should more directly manage internal slates rather than just adding divs and
//    then having them filled in.
//  * TextTables should be created that simply use text in cells, radically speeding up
//    printing of such tables (and covering 80% of use cases).
//  * IDEALLY: Make a single table that will look at what each cell is pointing to (table
//    or text) and write out what it needs to, in place.
//  * Add a ClearColumn method, as well as other column functionality.


#ifndef EMP_WEB_TABLE_H
#define EMP_WEB_TABLE_H

#include "../base/vector.h"

#include "Slate.h"
#include "Widget.h"

namespace emp {
namespace web {

  namespace internal {

    struct TableRow;
    class TableInfo;

    struct TableElement {
      Style style;       // CSS Style
      Attributes attr;   // HTML Attributes about a cell.
      Listeners listen;  // Listen for web events

      bool IsAnnotated() { return style || attr || listen; }
      void Apply(const std::string & name) { style.Apply(name); attr.Apply(name); listen.Apply(name); }
    };

    struct TableData : public TableElement {
      size_t colspan=1;    // How many columns wide is this TableData?
      size_t rowspan=1;    // How many rows deep is this TableData?
      bool header=false;   // Is this TableData a header (<th> vs <td>)?
      bool masked=false;   // Is this cell masked by another cell?

      emp::vector<Widget> children;  // Widgets contained in this cell.

      bool OK(std::stringstream & ss, bool verbose=false, const std::string & prefix="") {
        bool ok = true;
        if (verbose) ss << prefix << "Scanning: emp::TableData" << std::endl;
        if (masked) { ss << "Warning: Masked cell may have contents!" << std::endl; ok = false; }
        return ok;
      }
    };  // END: TableData


    struct TableRow : public TableElement {
      emp::vector<TableData> data;  // detail object for each cell in this row.

      // Apply to all cells in row.
      template <typename SETTING_TYPE>
      TableRow & CellsCSS(const std::string & setting, SETTING_TYPE && value) {
        for (auto & datum : data) datum.style.Set(setting, value);
        return *this;
      }

      // Apply to specific cell in row.
      template <typename SETTING_TYPE>
      TableRow & CellCSS(size_t col_id, const std::string & setting, SETTING_TYPE && value) {
        data[col_id].style.Set(setting, value);
        return *this;
      }

      bool OK(std::stringstream & ss, bool verbose=false, const std::string & prefix="") {
        bool ok = true;
        if (verbose) { ss << prefix << "Scanning: emp::TableRow" << std::endl; }
        for (auto & cell : data) ok = ok && cell.OK(ss, verbose, prefix+"  ");
        return ok;
      }
    };

    struct TableCol : public TableElement { };  // Currently no column-specific info!

    // Group of rows or columns...
    struct TableGroup : public TableElement {
      size_t span = 1;       // How many rows/columns does this group represent?
      bool masked = false;   // Is the current group masked because of a previous span?
    };

    class TableInfo : public internal::WidgetInfo {
      friend Table;
    protected:
      size_t row_count;                    // How big is this table?
      size_t col_count;
      emp::vector<TableRow> rows;          // Detail object for each row
      emp::vector<TableCol> cols;          // Detail object for each column (if needed)
      emp::vector<TableGroup> col_groups;  // Detail object for each column group (if needed)
      emp::vector<TableGroup> row_groups;  // Detail object for each row group (if needed)

      size_t append_row;               // Which row is triggering an append?
      size_t append_col;               // Which col is triggering an append?

      TableInfo(const std::string & in_id="")
        : internal::WidgetInfo(in_id), row_count(0), col_count(0), append_row(0), append_col(0) { ; }
      TableInfo(const TableInfo &) = delete;               // No copies of INFO allowed
      TableInfo & operator=(const TableInfo &) = delete;   // No copies of INFO allowed
      virtual ~TableInfo() { ; }

      std::string TypeName() const override { return "TableInfo"; }
      virtual bool IsTableInfo() const override { return true; }

      void Resize(size_t new_rows, size_t new_cols) {
        // Resize preexisting rows if remaining
        if (new_cols != col_count) {
          for (size_t r = 0; r < rows.size() && r < new_rows; r++) {
            rows[r].data.resize(new_cols);
            for (size_t c = col_count; c < new_cols; c++) { AddChild(r, c, Text("")); }
          }
          col_count = new_cols;                    // Store the new column count

          // Resize extra column info, only if currently in use.
          if (cols.size()) cols.resize(new_cols);
          if (col_groups.size()) col_groups.resize(new_cols);
        }

        // Resize number of rows.
        if (new_rows != row_count) {
          rows.resize(new_rows);
          for (size_t r = row_count; r < new_rows; r++) {
            rows[r].data.resize(col_count);
            for (size_t c = 0; c < col_count; c++) { AddChild(r, c, Text("")); }
          }
          row_count = new_rows;

          // Resize extra row group info, only if needed.
          if (row_groups.size()) row_groups.resize(new_rows);
        }

      }

      void DoActivate(bool top_level=true) override {
        // Activate all of the cell children.
        for (size_t r = 0; r < row_count; r++) {
          for (size_t c = 0; c < col_count; c++) {
            for (auto & child : rows[r].data[c].children) child->DoActivate(false);
          }
        }

        // Activate this Table.
        internal::WidgetInfo::DoActivate(top_level);
      }


      // Return a text element for appending into a specific cell (use existing one or build new)
      web::Text & GetTextWidget(size_t r, size_t c) {
        auto & cell_children = rows[r].data[c].children;
        // If final element in this cell doesn't exists, isn't text, or can't append, but new Text!
        if (cell_children.size() == 0
            || cell_children.back().IsText() == false
            || cell_children.back().AppendOK() == false)  {
          AddChild(Text());
        }
        return (Text &) cell_children.back();
      }

      web::Text & GetTextWidget() {
        // Make sure the number of rows hasn't changed, making the current position illegal.
        if (append_row >= row_count) append_row = 0;
        if (append_col >= col_count) append_col = 0;

        return GetTextWidget(append_row, append_col);
      }

      // Append into the current cell
      Widget Append(Widget in) override { AddChild(in); return in; }
      Widget Append(const std::string & text) override { return GetTextWidget() << text; }
      Widget Append(const std::function<std::string()> & in_fun) override {
        return GetTextWidget() << in_fun;
      }

      // Add a widget to the specified cell in the current table.
      void AddChild(size_t r, size_t c, Widget in) {
        emp_assert(in->parent == nullptr && "Cannot insert widget if already has parent!", in->id);
        emp_assert(in->state != Widget::ACTIVE && "Cannot insert a stand-alone active widget!");

        // Setup parent-child relationship in the specified cell.
        rows[r].data[c].children.emplace_back(in);
        in->parent = this;
        Register(in);

        // If this element (as new parent) is active, anchor widget and activate it!
        if (state == Widget::ACTIVE) {
          // Create a span tag to anchor the new widget.
          std::string cell_id = emp::to_string(id, '_', r, '_', c);
          EM_ASM_ARGS({
              parent_id = Pointer_stringify($0);
              child_id = Pointer_stringify($1);
              $('#' + parent_id).append('<span id="' + child_id + '"></span>');
            }, cell_id.c_str(), in.GetID().c_str());

          // Now that the new widget has some place to hook in, activate it!
          in->DoActivate();
        }
      }

      // If no cell is specified for AddChild, use the current cell.
      void AddChild(Widget in) {
        // Make sure the number of rows hasn't changed, making the current position illegal.
        if (append_row >= row_count) append_row = 0;
        if (append_col >= col_count) append_col = 0;

        AddChild(append_row, append_col, in);
      }


      // Tables need to facilitate recursive registrations

      void RegisterChildren(internal::SlateInfo * regestrar) override {
        for (size_t r = 0; r < row_count; r++) {
          for (size_t c = 0; c < col_count; c++) {
            for (Widget & child : rows[r].data[c].children) regestrar->Register(child);
          }
        }
      }

      void UnregisterChildren(internal::SlateInfo * regestrar) override {
        for (size_t r = 0; r < row_count; r++) {
          for (size_t c = 0; c < col_count; c++) {
            for (Widget & child : rows[r].data[c].children) regestrar->Unregister(child);
          }
        }
      }

      virtual void GetHTML(std::stringstream & HTML) override {
        emp_assert(cols.size() == 0 || cols.size() == col_count);
        emp_assert(col_groups.size() == 0 || col_groups.size() == col_count);

        HTML.str("");                                           // Clear the current text.
        HTML << "<table id=\"" << id << "\">";

        // Include column/row details only as needed.
        const bool use_colg = col_groups.size();
        const bool use_cols = cols.size();
        const bool use_rowg = row_groups.size();

        if (use_colg || use_cols) {
          for (size_t c = 0; c < col_count; ++c) {
            if (use_colg && col_groups[c].masked == false) {
              HTML << "<colgroup";
              if (col_groups[c].IsAnnotated()) HTML << " id=" << id << "_cg" << c;
              HTML << ">";
            }
            HTML << "<col";
            if (use_cols && cols[c].IsAnnotated()) HTML << " id=" << id << "_c" << c;
            HTML << ">";
          }
        }

        // Loop through all of the rows in the table.
        for (size_t r = 0; r < rows.size(); r++) {
          if (use_rowg && row_groups[r].masked == false) {
            HTML << "<tbody";
            if (row_groups[r].IsAnnotated()) HTML << " id=" << id << "_rg" << r;
            HTML << ">";
          }

          auto & row = rows[r];
          HTML << "<tr";
          if (row.IsAnnotated()) HTML << " id=" << id << '_' << r;
          HTML << ">";

          // Loop through each cell in this row.
          for (size_t c = 0; c < row.data.size(); c++) {
            auto & datum = row.data[c];
            if (datum.masked) continue;  // If this cell is masked by another, skip it!

            // Print opening tag.
            HTML << (datum.header ? "<th" : "<td");

            // Include an id for this cell if we have one.
            if (datum.IsAnnotated()) HTML << " id=" << id << '_' << r << '_' << c;

            // If this cell spans multiple rows or columns, indicate!
            if (datum.colspan > 1) HTML << " colspan=\"" << datum.colspan << "\"";
            if (datum.rowspan > 1) HTML << " rowspan=\"" << datum.rowspan << "\"";

            HTML << ">";

            // Loop through all children of this cell and build a span element for each.
            for (Widget & w : datum.children) {
              HTML << "<span id=\'" << w.GetID() << "'></span>";
            }

            // Print closing tag.
            HTML << (datum.header ? "</th>" : "</td>");
          }

          HTML << "</tr>";
        }

        HTML << "</table>";
      }

      void ClearCell(size_t row_id, size_t col_id) {
        auto & datum = rows[row_id].data[col_id];
        datum.colspan = 1;
        datum.rowspan = 1;
        datum.header = false;
        datum.masked = false;  // @CAO Technically, cell might still be masked!
        datum.style.Clear();
        datum.attr.Clear();
        datum.listen.Clear();

        // Clear out this cell's children.   @CAO: Keep a starting text widget if we can?
        if (parent) for (Widget & child : datum.children) parent->Unregister(child);
        datum.children.resize(0);
      }
      void ClearRowCells(size_t row_id) {
        for (size_t col_id = 0; col_id < col_count; col_id++) ClearCell(row_id, col_id);
      }
      void ClearRow(size_t row_id) {
        rows[row_id].style.Clear();
        rows[row_id].attr.Clear();
        rows[row_id].listen.Clear();
        ClearRowCells(row_id);
      }
      void ClearTableCells() { for (size_t r = 0; r < row_count; r++) ClearRowCells(r); }
      void ClearTableRows() { for (size_t r = 0; r < row_count; r++) ClearRow(r); }
      void ClearTable() { style.Clear(); attr.Clear(); listen.Clear(); Resize(0,0); }

      bool OK(std::stringstream & ss, bool verbose=false, const std::string & prefix="") {
        bool ok = true;

        // Basic info
        if (verbose) {
          ss << prefix << "Scanning: emp::TableInfo (rows=" << row_count
             << ", cols=" << col_count << ")." << std::endl;
        }

        // Make sure rows and columns are being sized correctly.
        if (row_count != rows.size()) {
          ss << prefix << "Error: row_count = " << row_count
             << ", but rows has " << rows.size() << " elements." << std::endl;
          ok = false;
        }

        if (cols.size() && col_count != cols.size()) {
          ss << prefix << "Error: col_count = " << col_count
             << ", but cols has " << cols.size() << " elements." << std::endl;
          ok = false;
        }

        if (row_count < 1) {
          ss << prefix << "Error: Cannot have " << row_count
             << " rows in table." << std::endl;
          ok = false;
        }

        if (col_count < 1) {
          ss << prefix << "Error: Cannot have " << col_count
             << " cols in table." << std::endl;
          ok = false;
        }

        // And perform the same test for row/column groups.
        if (col_groups.size() && col_count != col_groups.size()) {
          ss << prefix << "Error: col_count = " << col_count
             << ", but col_groups has " << col_groups.size() << " elements." << std::endl;
          ok = false;
        }

        if (row_groups.size() && row_count != row_groups.size()) {
          ss << prefix << "Error: row_count = " << row_count
             << ", but row_groups has " << row_groups.size() << " elements." << std::endl;
          ok = false;
        }

        // Recursively call OK on rows and data.
        for (size_t r = 0; r < row_count; r++) {
          ok = ok && rows[r].OK(ss, verbose, prefix+"  ");
          if (col_count != rows[r].data.size()) {
            ss << prefix << "  Error: col_count = " << col_count
               << ", but row has " << rows[r].data.size() << " elements." << std::endl;
            ok = false;
          }
          for (size_t c = 0; c < col_count; c++) {
            auto & cell = rows[r].data[c];
            if (c + cell.colspan > col_count) {
              ss << prefix << "  Error: Cell at row " << r << ", col " << c
                 << " extends past right side of table." << std::endl;
              ok = false;
            }
            if (r + cell.rowspan > row_count) {
              ss << prefix << "  Error: Cell at row " << r << ", col " << c
                 << " extends past bottom of table." << std::endl;
              ok = false;
            }
          }
        }

        return ok;
      }

      void ReplaceHTML() override {
        emp_assert(cols.size() == 0 || cols.size() == col_count);
        emp_assert(col_groups.size() == 0 || col_groups.size() == col_count);
        emp_assert(row_groups.size() == 0 || row_groups.size() == row_count);

        // Replace Table's HTML...
        internal::WidgetInfo::ReplaceHTML();

        // Then replace cells
        for (size_t r = 0; r < row_count; r++) {
          rows[r].Apply(emp::to_string(id, '_', r));
          for (size_t c = 0; c < col_count; c++) {
            auto & datum = rows[r].data[c];
            if (datum.masked) continue;  // If this cell is masked by another, skip it!
            datum.Apply(emp::to_string(id, '_', r, '_', c));

            // If this widget is active, immediately replace children.
            if (state == Widget::ACTIVE) {
              for (auto & child : datum.children) child->ReplaceHTML();
            }
          }
        }

        // And setup columns, column groups, and row groups, as needed.
        if (cols.size()) {
          for (size_t c = 0; c < col_count; c++) {
            if (cols[c].style.GetSize()==0) continue;
            cols[c].Apply(emp::to_string(id, "_c", c));
          }
        }
        if (col_groups.size()) {
          for (size_t c = 0; c < col_count; c++) {
            if (col_groups[c].masked || col_groups[c].style.GetSize()==0) continue;
            col_groups[c].Apply(emp::to_string(id, "_cg", c));
          }
        }
        if (row_groups.size()) {
          for (size_t c = 0; c < col_count; c++) {
            if (row_groups[c].masked || row_groups[c].style.GetSize()==0) continue;
            row_groups[c].Apply(emp::to_string(id, "_rg", c));
          }
        }
      }

    public:
      virtual std::string GetType() override { return "web::TableInfo"; }
    }; // end TableInfo


  } // end namespace internal


  class Table : public internal::WidgetFacet<Table> {
    friend class internal::TableInfo;
  protected:
    size_t cur_row;      // Which row/col is currently active?
    size_t cur_col;

    // A table's state determines how some operations work.
    enum state_t { TABLE, ROW, CELL, COL, COL_GROUP, ROW_GROUP };
    state_t state;

    // Get a properly cast version of indo.
    internal::TableInfo * Info() { return (internal::TableInfo *) info; }
    const internal::TableInfo * Info() const { return (internal::TableInfo *) info; }

    Table(internal::TableInfo * in_info, size_t _row=0, size_t _col=0, state_t _state=TABLE)
     : WidgetFacet(in_info), cur_row(_row), cur_col(_col), state(_state) { ; }

    // Apply to appropriate component based on current state.
    void DoCSS(const std::string & setting, const std::string & value) override {
      switch (state) {
      case TABLE:
        WidgetFacet<Table>::DoCSS(setting, value);
        break;
      case ROW:
        Info()->rows[cur_row].style.Set(setting, value);
        if (IsActive()) Info()->ReplaceHTML();   // @CAO only should replace cell's CSS
        break;
      case CELL:
        Info()->rows[cur_row].data[cur_col].style.Set(setting, value);
        if (IsActive()) Info()->ReplaceHTML();   // @CAO only should replace cell's CSS
        break;
      case COL:
        // If we haven't setup columns at all yet, do so.
        if (Info()->cols.size() == 0) Info()->cols.resize(GetNumCols());
        Info()->cols[cur_col].style.Set(setting, value);
        break;
      case COL_GROUP:
        // If we haven't setup column groups at all yet, do so.
        if (Info()->col_groups.size() == 0) Info()->col_groups.resize(GetNumCols());
        Info()->col_groups[cur_col].style.Set(setting, value);
        break;
      case ROW_GROUP:
        // If we haven't setup row groups at all yet, do so.
        if (Info()->row_groups.size() == 0) Info()->row_groups.resize(GetNumRows());
        Info()->row_groups[cur_row].style.Set(setting, value);
        break;
      default:
        emp_assert(false && "Table in unknown state!");
      };
    }

  public:
    Table(size_t r, size_t c, const std::string & in_id="")
      : WidgetFacet(in_id), cur_row(0), cur_col(0), state(TABLE)
    {
      emp_assert(c > 0 && r > 0);              // Ensure that we have rows and columns!

      info = new internal::TableInfo(in_id);
      Info()->Resize(r, c);
    }
    Table(const Table & in)
      : WidgetFacet(in), cur_row(in.cur_row), cur_col(in.cur_col), state(in.state) {
      emp_assert(state == TABLE || state == ROW || state == CELL
                 || state == COL || state == COL_GROUP || state == ROW_GROUP, state);
    }
    Table(const Widget & in) : WidgetFacet(in), cur_row(0), cur_col(0), state(TABLE) {
      emp_assert(info->IsTableInfo());
    }
    virtual ~Table() { ; }

    using INFO_TYPE = internal::TableInfo;

    size_t GetNumCols() const { return Info()->col_count; }
    size_t GetNumRows() const { return Info()->row_count; }
    size_t GetNumCells() const { return Info()->col_count*Info()->row_count; }

    // Called before an append.
    virtual void PrepareAppend() override {
      Info()->append_row = cur_row;
      Info()->append_col = cur_col;
    }

    size_t GetCurRow() const { return cur_row; }
    size_t GetCurCol() const { return cur_col; }

    bool InStateTable() const { return state == TABLE; }
    bool InStateRowGroup() const { return state == ROW_GROUP; }
    bool InStateColGroup() const { return state == COL_GROUP; }
    bool InStateRow() const { return state == ROW; }
    bool InStateCol() const { return state == COL; }
    bool InStateCell() const { return state == CELL; }

    Table & Clear() {
      // Clear based on tables current state.
      if (state == TABLE) Info()->ClearTable();
      else if (state == ROW) Info()->ClearRow(cur_row);
      // @CAO Make work for state == COL, COL_GROUP, or ROW_GROUP
      else if (state == CELL) Info()->ClearCell(cur_row, cur_col);
      else emp_assert(false && "Table in unknown state!", state);
      return *this;
    }
    Table & ClearTable() { Info()->ClearTable(); return *this; }
    Table & ClearRows() { Info()->ClearTableRows(); return *this; }
    Table & ClearRow(size_t r) { Info()->ClearRow(r); return *this; }
    Table & ClearCells() {
      if (state == TABLE) Info()->ClearTableCells();
      else if (state == ROW) Info()->ClearRowCells(cur_row);
      // @CAO Make work for state == COL, COL_GROUP, or ROW_GROUP
      else if (state == CELL) Info()->ClearCell(cur_row, cur_col);
      else emp_assert(false && "Unknown State!", state);
      return *this;
    }
    Table & ClearCell(size_t r, size_t c) { Info()->ClearCell(r, c); return *this; }

    // Functions to resize the number of rows, columns, or both!
    Table & Rows(size_t r) {
      Info()->Resize(r, Info()->col_count);
      if (cur_row >= r) cur_row = 0;
      return *this;
    }
    Table & Cols(size_t c) {
      Info()->Resize(Info()->row_count, c);
      if (cur_col >= c) cur_col = 0;
      return *this;
    }
    Table & Resize(size_t r, size_t c) {
      Info()->Resize(r, c);
      if (cur_row >= r) cur_row = 0;
      if (cur_col >= c) cur_col = 0;
      return *this;
    }

    Table GetCell(size_t r, size_t c) {
      emp_assert(Info() != nullptr);
      emp_assert(r < Info()->row_count && c < Info()->col_count,
                 r, c, Info()->row_count, Info()->col_count, GetID());
      return Table(Info(), r, c, CELL);
    }
    Table GetRow(size_t r) {
      emp_assert(r < Info()->row_count, r, Info()->row_count, GetID());
      return Table(Info(), r, 0, ROW);
    }
    Table GetCol(size_t c) {
      emp_assert(c < Info()->col_count, c, Info()->col_count, GetID());
      return Table(Info(), 0, c, COL);
    }
    Table GetRowGroup(size_t r) {
      emp_assert(r < Info()->row_count, r, Info()->row_count, GetID());
      return Table(Info(), r, 0, ROW_GROUP);
    }
    Table GetColGroup(size_t c) {
      emp_assert(c < Info()->col_count, c, Info()->col_count, GetID());
      return Table(Info(), 0, c, COL_GROUP);
    }
    Table GetTable() {
      return Table(Info(), cur_row, cur_col, TABLE);
    }

    web::Text GetTextWidget() { return Info()->GetTextWidget(); }

    // Update the current table object to change the active cell.
    Table & SetCellActive(size_t r, size_t c) {
      emp_assert(Info() != nullptr);
      emp_assert(r < Info()->row_count && c < Info()->col_count,
                 r, c, Info()->row_count, Info()->col_count, GetID());
      cur_row = r; cur_col = c;
      state = CELL;
      return *this;
    }
    Table & SetRowActive(size_t r) {
      emp_assert(r < Info()->row_count, r, Info()->row_count, GetID());
      cur_row = r; cur_col = 0;
      state = ROW;
      return *this;
    }
    Table & SetColActive(size_t c) {
      emp_assert(c < Info()->col_count, c, Info()->col_count, GetID());
      cur_col = c; cur_row = 0;
      state = COL;
      return *this;
    }
    Table & SetRowGroupActive(size_t r) {
      emp_assert(r < Info()->row_count, r, Info()->row_count, GetID());
      cur_row = r; cur_col = 0;
      state = ROW_GROUP;
      return *this;
    }
    Table & SetColGroupActive(size_t c) {
      emp_assert(c < Info()->col_count, c, Info()->col_count, GetID());
      cur_col = c; cur_row = 0;
      state = COL_GROUP;
      return *this;
    }
    Table & SetTableActive() { // Set focus to table; leave row and col where they are.
      state = TABLE;
      return *this;
    }

    Table & SetHeader(bool _h=true) {
      emp_assert(state == CELL);
      Info()->rows[cur_row].data[cur_col].header = _h;
      if (IsActive()) Info()->ReplaceHTML();   // @CAO only should replace cell's CSS
      return *this;
    }

    Widget AddText(size_t r, size_t c, const std::string & text) {
      GetCell(r,c) << text;
      return *this;
    }

    Widget AddHeader(size_t r, size_t c, const std::string & text) {
      GetCell(r,c) << text;
      SetHeader();
      return *this;
    }

    // // Setup functions on events associated with portions of a table.
    // RETURN_TYPE & OnCell(const std::string & event_name, const std::function<void(int,int)> & fun) {
    //   emp_assert(info != nullptr);
    //   info->listen.Set2Index(event_name, fun);
    //   return (RETURN_TYPE &) *this;
    // }

    // Apply to appropriate component based on current state.
    using WidgetFacet<Table>::SetCSS;
    std::string GetCSS(const std::string & setting) override {
      if (state == CELL) return Info()->rows[cur_row].data[cur_col].style.Get(setting);
      if (state == ROW) return Info()->rows[cur_row].style.Get(setting);
      if (state == COL) return Info()->cols[cur_col].style.Get(setting);
      if (state == ROW_GROUP) return Info()->row_groups[cur_row].style.Get(setting);
      if (state == COL_GROUP) return Info()->col_groups[cur_col].style.Get(setting);
      if (state == TABLE) return Info()->style.Get(setting);
      return "";
    }

    // Allow the row and column span of the current cell to be adjusted.
    Table & SetRowSpan(size_t new_span) {
      emp_assert((cur_row + new_span <= GetNumRows()) && "Row span too wide for table!");
      emp_assert(state == CELL || state == ROW_GROUP);

      if (state == CELL) {
        auto & datum = Info()->rows[cur_row].data[cur_col];
        const size_t old_span = datum.rowspan;
        const size_t col_span = datum.colspan;
        datum.rowspan = new_span;

        // For each col, make sure NEW rows are masked!
        for (size_t row = cur_row + old_span; row < cur_row + new_span; row++) {
          for (size_t col = cur_col; col < cur_col + col_span; col++) {
            Info()->rows[row].data[col].masked = true;
          }
        }

        // For each row, make sure former columns are unmasked!
        for (size_t row = cur_row + new_span; row < cur_row + old_span; row++) {
          for (size_t col = cur_col; col < cur_col + col_span; col++) {
            Info()->rows[row].data[col].masked = false;
          }
        }
      }

      else if (state == ROW_GROUP) {
        // If we haven't setup columns at all yet, do so.
        if (Info()->row_groups.size() == 0) Info()->row_groups.resize(GetNumRows());

        const size_t old_span = Info()->row_groups[cur_row].span;
        Info()->row_groups[cur_row].span = new_span;

        if (old_span != new_span) {
          for (size_t i=old_span; i<new_span; i++) { Info()->row_groups[cur_row+i].masked = true; }
          for (size_t i=new_span; i<old_span; i++) { Info()->row_groups[cur_row+i].masked = false; }
        }
      }

      // Redraw the entire table to fix row span information.
      if (IsActive()) Info()->ReplaceHTML();

      return *this;
    }

    Table & SetColSpan(size_t new_span) {
      emp_assert((cur_col + new_span <= GetNumCols()) && "Col span too wide for table!",
                 cur_col, new_span, GetNumCols(), GetID());
      emp_assert(state == CELL || state == COL_GROUP);

      if (state == CELL) {
        auto & datum = Info()->rows[cur_row].data[cur_col];
        const size_t old_span = datum.colspan;
        const size_t row_span = datum.rowspan;
        datum.colspan = new_span;

        // For each row, make sure new columns are masked!
        for (size_t row = cur_row; row < cur_row + row_span; row++) {
          for (size_t col = cur_col + old_span; col < cur_col + new_span; col++) {
            Info()->rows[row].data[col].masked = true;
          }
        }

        // For each row, make sure former columns are unmasked!
        for (size_t row = cur_row; row < cur_row + row_span; row++) {
          for (size_t col = cur_col + new_span; col < cur_col + old_span; col++) {
            Info()->rows[row].data[col].masked = false;
          }
        }
      }

      else if (state == COL_GROUP) {
        // If we haven't setup columns at all yet, do so.
        if (Info()->col_groups.size() == 0) Info()->col_groups.resize(GetNumCols());

        const size_t old_span = Info()->col_groups[cur_col].span;
        Info()->col_groups[cur_col].span = new_span;

        if (old_span != new_span) {
          for (size_t i=old_span; i<new_span; i++) { Info()->col_groups[cur_col+i].masked = true; }
          for (size_t i=new_span; i<old_span; i++) { Info()->col_groups[cur_col+i].masked = false; }
        }
      }

      // Redraw the entire table to fix col span information.
      if (IsActive()) Info()->ReplaceHTML();

      return *this;
    }

    Table & SetSpan(size_t new_span) {
      if (state == ROW_GROUP) return SetRowSpan(new_span);
      if (state == COL_GROUP) return SetColSpan(new_span);
      emp_assert(false);  // No other state should be allowd.
      return *this;
    }

    Table & SetSpan(size_t row_span, size_t col_span) {
      emp_assert(state == CELL);
      // @CAO Can do this more efficiently, but probably not worth it.
      SetRowSpan(row_span);
      SetColSpan(col_span);
      return *this;
    }

    // Apply to target row.
    template <typename SETTING_TYPE>
    Table & RowCSS(size_t row_id, const std::string & setting, SETTING_TYPE && value) {
      emp_assert(row_id >= 0 && row_id < Info()->row_count);
      Info()->rows[row_id].style.Set(setting, value);
      if (IsActive()) Info()->ReplaceHTML();   // @CAO only should replace row's CSS
      return *this;
    }

    // Apply to target cell.
    template <typename SETTING_TYPE>
    Table & CellCSS(size_t row_id, size_t col_id, const std::string & setting, SETTING_TYPE && value) {
      emp_assert(row_id >= 0 && row_id < Info()->row_count);
      emp_assert(col_id >= 0 && col_id < Info()->row_count);
      Info()->rows[row_id].style.Set(setting, value);
      if (IsActive()) Info()->ReplaceHTML();   // @CAO only should replace cell's CSS
      return *this;
    }

    // Apply to all rows.  (@CAO: Should we use fancier jquery here?)
    template <typename SETTING_TYPE>
    Table & RowsCSS(const std::string & setting, SETTING_TYPE && value) {
      for (auto & row : Info()->rows) row.style.Set(setting, emp::to_string(value));
      if (IsActive()) Info()->ReplaceHTML();
      return *this;
    }

    // Apply to all rows.  (@CAO: Should we use fancier jquery here?)
    template <typename SETTING_TYPE>
    Table & CellsCSS(const std::string & setting, SETTING_TYPE && value) {
      for (auto & row : Info()->rows) row.CellsCSS(setting, emp::to_string(value));
      if (IsActive()) Info()->ReplaceHTML();
      return *this;
    }

    virtual bool OK(std::stringstream & ss, bool verbose=false, const std::string & prefix="") {
      bool ok = true;

      // Basic info
      if (verbose) {
        ss << prefix << "Scanning: emp::Table (rows=" << Info()->row_count
           << ", cols=" << Info()->col_count << ")." << std::endl;
      }

      // Make sure current row and col are valid.
      if (cur_row >= Info()->row_count) {
        ss << prefix << "Error: cur_row = " << cur_row << "." << std::endl;
        ok = false;
      }

      if (cur_col >= Info()->col_count) {
        ss << prefix << "Error: cur_col = " << cur_col << "." << std::endl;
        ok = false;
      }

      // Make sure internal info is okay.
      ok = ok && Info()->OK(ss, verbose, prefix+"  ");

      return ok;
    }
  };


}
}

#endif
