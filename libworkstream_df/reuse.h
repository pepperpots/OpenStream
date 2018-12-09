#ifndef REUSE_H
#define REUSE_H

#include "wstream_df.h"

static inline int is_reuse_view(wstream_df_view_p v)
{
	return (v->reuse_associated_view != NULL);
}

static inline int reuse_view_has_reuse_predecessor(wstream_df_view_p v)
{
	return (v->reuse_data_view != NULL && is_reuse_view(v->reuse_data_view));
}

static inline int reuse_view_has_own_data(wstream_df_view_p v)
{
	return !v->reuse_data_view;
}

void __built_in_wstream_df_prepare_data(void* v);
void __built_in_wstream_df_reuse_update_data(void* v);
void match_reuse_output_clause_with_reuse_input_clause(wstream_df_view_p out_view, wstream_df_view_p in_view);
void match_reuse_input_clause_with_output_clause(wstream_df_view_p out_view, wstream_df_view_p in_view);
void match_reuse_output_clause_with_input_clause(wstream_df_view_p out_view, wstream_df_view_p in_view);

void check_reuse_copy(wstream_df_frame_p fp);

#endif
