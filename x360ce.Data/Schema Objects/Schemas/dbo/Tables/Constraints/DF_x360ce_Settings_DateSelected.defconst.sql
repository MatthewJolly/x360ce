﻿ALTER TABLE [dbo].[x360ce_Settings]
    ADD CONSTRAINT [DF_x360ce_Settings_DateSelected] DEFAULT (getdate()) FOR [DateSelected];
